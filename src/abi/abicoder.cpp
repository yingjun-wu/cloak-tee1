#include "abicoder.h"
#include "vector"

void abicoder::insert(UINT8ARRAY &coder,const UINT8ARRAY &input, size_t offset) {
    for(size_t i= offset, x = 0; x< input.size(); x++, i++ ) {
        coder.at(i) = input.at(x);
    }
}


UINT8ARRAY abicoder::to_bytes(const std::string& _s, size_t offset, bool boolean) {
    auto s = eevm::strip(_s);
    UINT8ARRAY h(32);
    if(!boolean)
        h.resize(ceil(s.size() / 2.0));
    if(s.empty()) return h;
    for(size_t i=0; i<offset; i++) {
        h.at(i ) = 0;
    }
        
    for(size_t  x = 0; x < s.size(); offset++, x+=2) {
        h.at(offset) = strtol(s.substr(x, 2).c_str(),0,16);          
    }
    return h;
}

UINT8ARRAY abicoder::uint256Coder(size_t size) {
    return UintNumber().encode(to_string(size));
}

UINT8ARRAY abicoder::fixed_to_bytes(const std::string &_s) {
    UINT8ARRAY h(32);
    auto s = Utils::BinaryToHex(_s);
    for(size_t  x = 0,offset=0; x < s.size(); offset++, x+=2) {
        h.at(offset) = strtol(s.substr(x, 2).c_str(),0,16);          
    }
    return h;
}

UINT8ARRAY abicoder::string_to_bytes(const std::string& _s) {
    auto s = Utils::BinaryToHex(_s);
    UINT8ARRAY h(ceil(s.size() / 2.0));
    if(s.empty()) return h;
    for(size_t offset=0, x = 0; x < s.size(); offset++, x+=2) {
        h.at(offset) = strtol(s.substr(x, 2).c_str(),0,16);    
    }
    return h;
}

UINT8ARRAY abicoder::encodeDynamicBytes(const UINT8ARRAY& value) {
    UINT8ARRAY result(32 + alignSize(value.size()));
    auto header = UintNumber().encode(to_string(value.size()));
    to_array(result, header);
    to_array(result, value, 32);
    return result;
}

UINT8ARRAY abicoder::basic_pack(const vector<PackParams>& parts) {
    size_t staticSize = 0, dynamicSize = 0;
    for(auto part : parts) {
        if(part.Dynamic) {
            staticSize +=32;
            dynamicSize += alignSize(part.data.size());
        } else {
            staticSize += alignSize(part.data.size());
        }
    }

    size_t offset = 0, dynamicOffset = staticSize;
    UINT8ARRAY data(staticSize + dynamicSize);

    for(auto part : parts) {
        if(part.Dynamic) {
            to_array(data, uint256Coder(dynamicOffset), offset);
            offset +=32;
            to_array(data, part.data, dynamicOffset);
            dynamicOffset += alignSize(part.data.size());
        } else {
            to_array(data, part.data, offset);
            offset += alignSize(part.data.size());
        }
    }
    return data;
}

UINT8ARRAY abicoder::pack(const std::vector<void*>& coders) {
    vector<abicoder::PackParams> parts;
    Coder* coder;
    for(size_t i=0; i<coders.size(); i++) {
        coder = (Coder*)coders[i];
        parts.push_back({coder->getDynamic(), coder->encode()});
        delete coder;
    }
    return basic_pack(parts);
}

UINT8ARRAY abicoder::pack(const std::vector<void*>& coders, const vector<ByteData> &value) {
    vector<PackParams> parts;
    Coder* coder;
    for(size_t i=0; i<coders.size(); i++) {
        coder = (Coder*)coders[i];
        coder->setValue(value[i]);
        parts.push_back({coder->getDynamic(), coder->encode()});
        delete coder;
    }
    return basic_pack(parts);
}

UINT8ARRAY abicoder::CoderArray::encode() {
    UINT8ARRAY result;
    result = UintNumber().encode(to_string(value.size()));
    
    vector<void*> coders;
    for (size_t i=0; i<value.size(); i++) {
        abicoder::paramCoder(coders, name, type, value[i], length);
    }
    auto data = pack(coders);
    result.insert(result.end(),data.begin(), data.end());
    return result;
}

void abicoder::paramCoder(vector<void*> &coders, const ByteData &name, const ByteData &_type,const ByteData & value) {
    auto [type, length, _] = Parsing(_type).result();
    paramCoder(coders, name, type, value, length);
}

void abicoder::paramCoder(vector<void*> &coders, const ByteData &name, const ByteData &_type,const vector<ByteData> & value) {
    auto [type, length, boolean] = Parsing(_type).result();
    if(boolean) {
        // array
        size_t len = length > 1 ? length : value.size();
        CoderArray* array = new CoderArray(name, type, len);
        array->setValue(value);
        coders.push_back(array);
        return;
    }
}

void abicoder::paramCoder(vector<void*> &coders, const ByteData &name, const ByteData &type,const ByteData & value, int length) {
    switch (contractType[type]){
    case ADDRESS: {
        CoderAddress* addr = new CoderAddress(name);
        addr->setValue(value);
        coders.push_back(addr);
        return;
    }
    case BOOL: {
        CoderBoolean* boolean =new  CoderBoolean(name);
        boolean->setValue(value);
        coders.push_back(boolean);
        break;
    }
    case STRING: {
        CoderString* str = new CoderString(name);
        str->setValue(value);
        coders.push_back(str);
        break;
    }
    case BYTES: {
        cout << length << name << endl;
        if(length == 0) {
            CoderDynamicBytes* dynamicBytes = new CoderDynamicBytes(name);
            dynamicBytes->setValue(value);
            coders.push_back(dynamicBytes);
        } else {
            CoderFixedBytes* dynamicBytes= new CoderFixedBytes(length);
            dynamicBytes->setValue(value);
            coders.push_back(dynamicBytes);
        }       
        break;
    }
    case UINT: {
        CoderNumber* number =new  CoderNumber(length, false);
        number->setValue(value);
        coders.push_back(number);
        break;
    }
    case INT: {
        CoderNumber* number1 = new CoderNumber(length, true);
        number1->setValue(value);
        coders.push_back(number1);
        break;
    }
    default:
        break;
    }
}