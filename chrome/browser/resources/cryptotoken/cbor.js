// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
class Cbor {
  constructor(buffer) {
    this.slice = new Uint8Array(buffer);
  }
  get data() {
    return this.slice;
  }
  get length() {
    return this.slice.length;
  }
  get empty() {
    return this.slice.length === 0;
  }
  get hex() {
    const hexTable = '0123456789abcdef';
    let s = '';
    for (let i = 0; i < this.data.length; i++) {
      s += hexTable.charAt(this.data[i] >> 4);
      s += hexTable.charAt(this.data[i] & 15);
    }
    return s;
  }
  base64Encode(chars, padding) {
    const len3 = 3 * Math.floor(this.slice.length / 3);
    var chunks = [];
    for (let i = 0; i < len3; i += 3) {
      const v =
          (this.slice[i] << 16) + (this.slice[i + 1] << 8) + this.slice[i + 2];
      chunks.push(
          chars[v >> 18] + chars[(v >> 12) & 0x3f] + chars[(v >> 6) & 0x3f] +
          chars[v & 0x3f]);
    }
    const remainder = this.slice.length - len3;
    if (remainder === 1) {
      const v = this.slice[len3];
      chunks.push(chars[v >> 2] + chars[(v << 4) & 0x3f]);
      if (padding === 1 /* Include */) {
        chunks.push('==');
      }
    } else if (remainder === 2) {
      const v = (this.slice[len3] << 8) + this.slice[len3 + 1];
      chunks.push(
          chars[v >> 10] + chars[(v >> 4) & 0x3f] + chars[(v << 2) & 0x3f]);
      if (padding === 1 /* Include */) {
        chunks.push('=');
      }
    }
    return chunks.join('');
  }
  webSafeBase64() {
    const chars =
        'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
    return this.base64Encode(chars, 0 /* None */);
  }
  base64() {
    const chars =
        'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
    return this.base64Encode(chars, 1 /* Include */);
  }
  compare(other) {
    if (this.length < other.length) {
      return -1;
    } else if (this.length > other.length) {
      return 1;
    }
    for (let i = 0; i < this.length; i++) {
      if (this.slice[i] < other.slice[i]) {
        return -1;
      } else if (this.slice[i] > other.slice[i]) {
        return 1;
      }
    }
    return 0;
  }
  getU8() {
    if (this.empty) {
      throw new Error('Cbor: empty during getU8');
    }
    const byte = this.slice[0];
    this.slice = this.slice.subarray(1);
    return byte;
  }
  skip(n) {
    if (this.length < n) {
      throw new Error('Cbor: too few bytes to skip');
    }
    this.slice = this.slice.subarray(n);
  }
  getBytes(n) {
    if (this.length < n) {
      throw new Error('Cbor: insufficient bytes in getBytes');
    }
    const ret = this.slice.subarray(0, n);
    this.slice = this.slice.subarray(n);
    return ret;
  }
  getUnsigned(n) {
    const bytes = this.getBytes(n);
    let value = 0;
    for (let i = 0; i < n; i++) {
      value <<= 8;
      value |= bytes[i];
    }
    return value;
  }
  getU16() {
    return this.getUnsigned(2);
  }
  getU32() {
    return this.getUnsigned(4);
  }
  getASN1_(expectedTag, includeHeader) {
    if (this.empty) {
      throw new Error('getASN1: empty slice, expected tag ' + expectedTag);
    }
    const v = this.getAnyASN1();
    if (v.tag !== expectedTag) {
      throw new Error('getASN1: got tag ' + v.tag + ', want ' + expectedTag);
    }
    if (!includeHeader) {
      v.val.skip(v.headerLen);
    }
    return v.val;
  }
  getASN1(expectedTag) {
    return this.getASN1_(expectedTag, false);
  }
  getASN1Element(expectedTag) {
    return this.getASN1_(expectedTag, true);
  }
  getOptionalASN1(expectedTag) {
    if (this.slice.length < 1 || this.slice[0] !== expectedTag) {
      return null;
    }
    return this.getASN1(expectedTag);
  }
  getAnyASN1() {
    const header = new Cbor(this.slice);
    const tag = header.getU8();
    const lengthByte = header.getU8();
    if ((tag & 0x1f) === 0x1f) {
      throw new Error('getAnyASN1: long-form tag found');
    }
    let len = 0;
    let headerLen = 0;
    if ((lengthByte & 0x80) === 0) {
      // Short form length.
      len = lengthByte + 2;
      headerLen = 2;
    } else {
      // The high bit indicates that this is the long form, while the next 7
      // bits encode the number of subsequent octets used to encode the length
      // (ITU-T X.690 clause 8.1.3.5.b).
      const numBytes = lengthByte & 0x7f;
      // Bitwise operations are always on signed 32-bit two's complement
      // numbers.  This check ensures that we stay under this limit.  We could
      // do this in a better way, but there's no need to process very large
      // objects.
      if (numBytes === 0 || numBytes > 3) {
        throw new Error('getAnyASN1: bad ASN.1 long-form length');
      }
      const lengthBytes = header.getBytes(numBytes);
      for (let i = 0; i < numBytes; i++) {
        len <<= 8;
        len |= lengthBytes[i];
      }
      if (len < 128 || (len >> ((numBytes - 1) * 8)) === 0) {
        throw new Error('getAnyASN1: incorrectly encoded ASN.1 length');
      }
      headerLen = 2 + numBytes;
      len += headerLen;
    }
    if (this.slice.length < len) {
      throw new Error('getAnyASN1: too few bytes in input');
    }
    const prefix = this.slice.subarray(0, len);
    this.slice = this.slice.subarray(len);
    return {tag: tag, headerLen: headerLen, val: new Cbor(prefix)};
  }
  getBase128Int() {
    let lookahead = this.slice.length;
    if (lookahead > 4) {
      lookahead = 4;
    }
    let len = 0;
    for (let i = 0; i < lookahead; i++) {
      if (!(this.slice[i] & 0x80)) {
        len = i + 1;
        break;
      }
    }
    if (len === 0) {
      throw new Error('base128 value too large');
    }
    let n = 0;
    let octets = this.getBytes(len);
    for (let i = 0; i < len; i++) {
      if ((n & 0xff000000) !== 0) {
        throw new Error('base128 value too large');
      }
      n <<= 7;
      n |= octets[i] & 0x7f;
    }
    return n;
  }
  getASN1ObjectIdentifier() {
    let b = this.getASN1(6 /* OBJECT */);
    let first = b.getBase128Int();
    let result = [0, 0];
    result[1] = first % 40;
    result[0] = (first - result[1]) / 40;
    while (!b.empty) {
      result.push(b.getBase128Int());
    }
    return result;
  }
  getCBORHeader() {
    const copy = new Cbor(this.slice);
    const a = this.getU8();
    const majorType = a >> 5;
    const info = a & 31;
    if (info < 24) {
      return [majorType, info, new Cbor(copy.getBytes(1))];
    } else if (info < 28) {
      const lengthLength = 1 << (info - 24);
      let data = this.getBytes(lengthLength);
      let value = 0;
      for (let i = 0; i < lengthLength; i++) {
        // Javascript has problems handling uint64s given the limited range of
        // a double.
        if (value > 35184372088831) {
          throw new Error('Cbor: cannot represent CBOR number');
        }
        // Not using bitwise operations to avoid truncating to 32 bits.
        value *= 256;
        value += data[i];
      }
      switch (lengthLength) {
        case 1:
          if (value < 24) {
            throw new Error(
                'Cbor: value should have been encoded in single byte');
          }
          break;
        case 2:
          if (value < 256) {
            throw new Error('Cbor: non-minimal integer');
          }
          break;
        case 4:
          if (value < 65536) {
            throw new Error('Cbor: non-minimal integer');
          }
          break;
        case 8:
          if (value < 4294967296) {
            throw new Error('Cbor: non-minimal integer');
          }
          break;
      }
      return [majorType, value, new Cbor(copy.getBytes(1 + lengthLength))];
    } else {
      throw new Error('Cbor: CBOR contains unhandled info value ' + info);
    }
  }
  getCBOR() {
    const [major, value] = this.getCBORHeader();
    switch (major) {
      case 0:
        return value;
      case 1:
        return 0 - (1 + value);
      case 2:
        return this.getBytes(value);
      case 3:
        return this.getBytes(value);
      case 4: {
        let ret = new Array(value);
        for (let i = 0; i < value; i++) {
          ret[i] = this.getCBOR();
        }
        return ret;
      }
      case 5:
        if (value === 0) {
          return {};
        }
        let copy = new Cbor(this.data);
        const [firstKeyMajor] = copy.getCBORHeader();
        if (firstKeyMajor === 3) {
          // String-keyed map.
          let lastKeyHeader = new Cbor(new Uint8Array(0));
          let lastKeyBytes = new Cbor(new Uint8Array(0));
          let ret = {};
          for (let i = 0; i < value; i++) {
            const [keyMajor, keyLength, keyHeader] = this.getCBORHeader();
            if (keyMajor !== 3) {
              throw new Error('Cbor: non-string in string-valued map');
            }
            const keyBytes = new Cbor(this.getBytes(keyLength));
            if (i > 0) {
              const headerCmp = lastKeyHeader.compare(keyHeader);
              if (headerCmp > 0 ||
                  (headerCmp === 0 && lastKeyBytes.compare(keyBytes) >= 0)) {
                throw new Error(
                    'Cbor: map keys in wrong order: ' + lastKeyHeader.hex +
                    '/' + lastKeyBytes.hex + ' ' + keyHeader.hex + '/' +
                    keyBytes.hex);
              }
            }
            lastKeyHeader = keyHeader;
            lastKeyBytes = keyBytes;
            ret[keyBytes.parseUTF8()] = this.getCBOR();
          }
          return ret;
        } else if (firstKeyMajor === 0 || firstKeyMajor === 1) {
          // Number-keyed map.
          let lastKeyHeader = new Cbor(new Uint8Array(0));
          let ret = {};
          for (let i = 0; i < value; i++) {
            let [keyMajor, keyValue, keyHeader] = this.getCBORHeader();
            if (keyMajor !== 0 && keyMajor !== 1) {
              throw new Error('Cbor: non-number in number-valued map');
            }
            if (i > 0 && lastKeyHeader.compare(keyHeader) >= 0) {
              throw new Error(
                  'Cbor: map keys in wrong order: ' + lastKeyHeader.hex + ' ' +
                  keyHeader.hex);
            }
            lastKeyHeader = keyHeader;
            if (keyMajor === 1) {
              keyValue = 0 - (1 + keyValue);
            }
            ret[keyValue] = this.getCBOR();
          }
          return ret;
        } else {
          throw new Error(
              'Cbor: map keyed by invalid major type ' + firstKeyMajor);
        }
      default:
        throw new Error('Cbor: unhandled major type ' + major);
    }
  }
  parseUTF8() {
    return (new TextDecoder('utf-8')).decode(this.slice);
  }
}
