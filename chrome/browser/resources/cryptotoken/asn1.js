// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ASN.1 parser, in the manner of BoringSSL's CBS (crypto byte string) lib.
 *
 * A |ByteString| is a buffer of DER-encoded bytes.  To decode the buffer, you
 * must know something about the expected sequence of tags, which allows you to
 * call getASN1() and friends with the right arguments and in the right order.
 *
 * https://commondatastorage.googleapis.com/chromium-boringssl-docs/bytestring.h.html
 * is the canonical API reference.
 */
const ByteString = class {
  /**
   * Creates a new ASN.1 parser.
   * @param {!Uint8Array} buffer DER-encoded ASN.1 bytes.
   */
  constructor(buffer) {
    /** @private {!Uint8Array} */
    this.slice_ = buffer;
  }

  /**
   * @return {!Uint8Array} The DER-encoded bytes remaining in the buffer.
   */
  get data() {
    return this.slice_;
  }

  /**
   * @return {number} The number of DER-encoded bytes remaining in the buffer.
   */
  get length() {
    return this.slice_.length;
  }

  /**
   * @return {boolean} True if the buffer is empty.
   */
  get empty() {
    return this.slice_.length === 0;
  }

  /**
   * Pops a byte from the start of the buffer.
   * @return {number} A byte.
   * @throws {Error} if the buffer is empty.
   * @private
   */
  getU8_() {
    if (this.empty) {
      throw Error('getU8_: slice empty');
    }
    const b = this.slice_[0];
    this.slice_ = this.slice_.subarray(1);
    return b;
  }

  /**
   * Pops |n| bytes from the buffer.
   * @param {number} n The number of bytes to pop.
   * @throws {Error}
   * @private
   */
  skip_(n) {
    if (this.slice_.length < n) {
      throw Error('skip_: too few bytes in input');
    }
    this.slice_ = this.slice_.subarray(n);
  }

  /**
   * @param {number} n The number of bytes to read from the buffer.
   * @return {!Uint8Array} an array of |n| bytes.
   * @throws {Error}
   */
  getBytes(n) {
    if (this.slice_.length < n) {
      throw Error('getBytes: too few bytes in input');
    }
    const prefix = this.slice_.subarray(0, n);
    this.slice_ = this.slice_.subarray(n);
    return prefix;
  }

  /**
   * Returns a value of the specified type.
   * @param {number} expectedTag The expected tag, e.g. |SEQUENCE|, of the next
   *     value in the buffer.
   * @param {boolean=} opt_includeHeader If true, include header bytes in the
   *     buffer.
   * @return {!ByteString} The DER-encoded value bytes.
   * @throws {Error}
   * @private
   */
  getASN1_(expectedTag, opt_includeHeader) {
    if (this.empty) {
      throw Error('getASN1: empty slice, expected tag ' + expectedTag);
    }
    const v = this.getAnyASN1();
    if (v.tag !== expectedTag) {
      throw Error('getASN1: got tag ' + v.tag + ', want ' + expectedTag);
    }
    if (!opt_includeHeader) {
      v.val.skip_(v.headerLen);
    }
    return v.val;
  }

  /**
   * Returns a value of the specified type.
   * @param {number} expectedTag The expected tag, e.g. |SEQUENCE|, of the next
   *     value in the buffer.
   * @return {!ByteString} The DER-encoded value bytes.
   * @throws {Error}
   */
  getASN1(expectedTag) {
    return this.getASN1_(expectedTag, false);
  }

  /**
   * Returns a base128-encoded integer.
   * @return {number} an int32.
   * @private
   */
  getBase128Int_() {
    var lookahead = this.slice_.length;
    if (lookahead > 4) {
      lookahead = 4;
    }
    var len = 0;
    for (var i = 0; i < lookahead; i++) {
      if (!(this.data[i] & 0x80)) {
        len = i + 1;
        break;
      }
    }
    if (len === 0) {
      throw Error('terminating byte not found');
    }
    var n = 0;
    var octets = this.getBytes(len);
    for (var i = 0; i < len; i++) {
      n |= (octets[i] & 0x7f) << 7 * (len - i - 1);
    }
    return n;
  }

  /**
   * Returns an OBJECT IDENTIFIER.
   * @return {Array<number>}
   */
  getASN1ObjectIdentifier() {
    var b = this.getASN1(Tag.OBJECT);
    var result = [];
    var first = b.getBase128Int_();
    result[1] = first % 40;
    result[0] = (first - result[1]) / 40;
    var n = 2;
    while (!b.empty) {
      result[n++] = b.getBase128Int_();
    }
    return result;
  }

  /**
   * Returns a value of the specified type, with its header.
   * @param {number} expectedTag The expected tag, e.g. |SEQUENCE|, of the next
   *     value in the buffer.
   * @return {!ByteString} The DER-encoded header and value bytes.
   * @throws {Error}
   */
  getASN1Element(expectedTag) {
    return this.getASN1_(expectedTag, true);
  }

  /**
   * Returns an optional value of the specified type.
   * @param {number} expectedTag The expected tag, e.g. |SEQUENCE|, of the next
   *     value in the buffer.
   * @return {ByteString}
   * */
  getOptionalASN1(expectedTag) {
    if (this.slice_.length < 1 || this.slice_[0] !== expectedTag) {
      return null;
    }
    return this.getASN1(expectedTag);
  }

  /**
   * Matches and returns any ASN.1 type.
   * @return {{tag: number, headerLen: number, val: !ByteString}} An ASN.1
   *    value.  The returned |ByteString| includes the DER header bytes.
   * @throws {Error}
   */
  getAnyASN1() {
    const header = new ByteString(this.slice_);
    const tag = header.getU8_();
    const lengthByte = header.getU8_();

    if ((tag & 0x1f) === 0x1f) {
      throw Error('getAnyASN1: long-form tag found');
    }

    var len = 0;
    var headerLen = 0;

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
        throw Error('getAnyASN1: bad ASN.1 long-form length');
      }
      const lengthBytes = header.getBytes(numBytes);
      for (var i = 0; i < numBytes; i++) {
        len <<= 8;
        len |= lengthBytes[i];
      }

      if (len < 128 || (len >> ((numBytes - 1) * 8)) === 0) {
        throw Error('getAnyASN1: incorrectly encoded ASN.1 length');
      }

      headerLen = 2 + numBytes;
      len += headerLen;
    }

    if (this.slice_.length < len) {
      throw Error('getAnyASN1: too few bytes in input');
    }
    const prefix = this.slice_.subarray(0, len);
    this.slice_ = this.slice_.subarray(len);
    return {tag: tag, headerLen: headerLen, val: new ByteString(prefix)};
  }
};

/**
 * Tag is a container for ASN.1 tag values, like |SEQUENCE|.  These values
 * are arguments to e.g. getASN1().
 */
const Tag = class {
  /** @return {number} */
  static get BOOLEAN() {
    return 1;
  }

  /** @return {number} */
  static get INTEGER() {
    return 2;
  }

  /** @return {number} */
  static get BITSTRING() {
    return 3;
  }

  /** @return {number} */
  static get OCTETSTRING() {
    return 4;
  }

  /** @return {number} */
  static get NULL() {
    return 5;
  }

  /** @return {number} */
  static get OBJECT() {
    return 6;
  }

  /** @return {number} */
  static get UTF8String() {
    return 12;
  }

  /** @return {number} */
  static get PrintableString() {
    return 19;
  }

  /** @return {number} */
  static get UTCTime() {
    return 23;
  }

  /** @return {number} */
  static get GeneralizedTime() {
    return 24;
  }

  /** @return {number} */
  static get CONSTRUCTED() {
    return 0x20;
  }

  /** @return {number} */
  static get SEQUENCE() {
    return 0x30;
  }

  /** @return {number} */
  static get SET() {
    return 0x31;
  }

  /** @return {number} */
  static get CONTEXT_SPECIFIC() {
    return 0x80;
  }
};

/**
 * ASN.1 builder, in the manner of BoringSSL's CBB (crypto byte builder).
 *
 * A |ByteBuilder| maintains a |Uint8Array| slice and appends to it on demand.
 * After appending all the necessary values, the |data| property returns a
 * slice containing the result. Utility functions are provided for appending
 * ASN.1 DER-formatted values.
 *
 * Several of the functions take a "continuation" parameter. This is a function
 * that makes calls to its argument in order to lay down the contents of a
 * value. Once the continuation returns, the length prefix will be serialised.
 * It is illegal to call methods on a parent ByteBuilder while a continuation
 * function is running.
 */
const ByteBuilder = class {
  constructor() {
    /** @private {?Uint8Array} */
    this.slice_ = null;
    /** @private {number} */
    this.len_ = 0;
    /** @private {?ByteBuilder} */
    this.child_ = null;
  }

  /**
   * @return {!Uint8Array} The constructed bytes
   */
  get data() {
    if (this.child_ != null) {
      throw Error('data access while child is pending');
    }
    if (this.slice_ === null) {
      return new Uint8Array(0);
    }
    return this.slice_.subarray(0, this.len_);
  }

  /**
   * Reallocates the slice to at least a given size.
   * @param {number} minNewSize The minimum resulting size of the slice.
   * @private
   */
  realloc_(minNewSize) {
    var newSize = 0;

    if (minNewSize > Number.MAX_SAFE_INTEGER - minNewSize) {
      // Cannot grow exponentially without overflow.
      newSize = minNewSize;
    } else {
      newSize = minNewSize * 2;
    }

    if (this.slice_ === null) {
      if (newSize < 128) {
        newSize = 128;
      }
      this.slice_ = new Uint8Array(newSize);
      return;
    }

    const newSlice = new Uint8Array(newSize);
    for (var i = 0; i < this.len_; i++) {
      newSlice[i] = this.slice_[i];
    }

    this.slice_ = newSlice;
  }

  /**
   * Extends the current slice by the given number of bytes.
   * @param {number} n The number of extra bytes needed in the slice.
   * @return {number} The offset of the new bytes.
   * @throws {Error}
   * @private
   */
  extend_(n) {
    if (this.child_ != null) {
      throw Error('write while child pending');
    }
    if (this.len_ > Number.MAX_SAFE_INTEGER - n) {
      throw Error('length overflow');
    }
    if (this.slice_ === null || this.len_ + n > this.slice_.length) {
      this.realloc_(this.len_ + n);
    }

    const offset = this.len_;
    this.len_ += n;
    return offset;
  }

  /**
   * Appends a uint8 to the slice.
   * @param {number} b The byte to append.
   * @throws {Error}
   * @private
   */
  addU8_(b) {
    const offset = this.extend_(1);
    this.slice_[offset] = b;
  }

  /**
   * Appends a length prefixed value to the slice.
   * @param {number} lenLen The number of length-prefix bytes.
   * @param {boolean} isASN1 True iff an ASN.1 length should be prefixed.
   * @param {function(ByteBuilder)} k A function to construct the contents.
   * @throws {Error}
   * @private
   */
  addLengthPrefixed_(lenLen, isASN1, k) {
    var offset = this.extend_(lenLen);
    var child = new ByteBuilder();
    child.slice_ = this.slice_;
    child.len_ = this.len_;
    this.child_ = child;
    k(child);

    var length = child.len_ - lenLen - offset;
    if (length > 0x7fffffff) {
      // If a number larger than this is used with a shift operation in
      // Javascript, the result is incorrect.
      throw Error('length too large');
    }

    if (isASN1) {
      // In the case of ASN.1 a single byte was reserved for
      // the length. The contents of the array may need to be
      // shifted along if the length needs more than that.
      if (lenLen !== 1) {
        throw Error('internal error');
      }

      var lenByte = 0;
      if (length > 0xffffff) {
        lenLen = 5;
        lenByte = 0x80 | 4;
      } else if (length > 0xffff) {
        lenLen = 4;
        lenByte = 0x80 | 3;
      } else if (length > 0xff) {
        lenLen = 3;
        lenByte = 0x80 | 2;
      } else if (length > 0x7f) {
        lenLen = 2;
        lenByte = 0x80 | 1;
      } else {
        lenLen = 1;
        lenByte = length;
        length = 0;
      }

      child.slice_[offset] = lenByte;
      const extraBytesNeeded = lenLen - 1;
      if (extraBytesNeeded > 0) {
        child.extend_(extraBytesNeeded);
        child.slice_.copyWithin(offset + lenLen, offset + 1, child.len_);
      }

      offset++;
      lenLen = extraBytesNeeded;
    }

    var l = length;
    for (var i = lenLen - 1; i >= 0; i--) {
      child.slice_[offset + i] = l;
      l >>= 8;
    }

    if (l !== 0) {
      throw Error('pending child length exceeds reserved space');
    }

    this.slice_ = child.slice_;
    this.len_ = child.len_;
    this.child_ = null;
  }

  /**
   * Appends an ASN.1 element to the slice.
   * @param {number} tag The ASN.1 tag value (must be < 31).
   * @param {function(ByteBuilder)} k A function to construct the contents.
   * @throws {Error}
   */
  addASN1(tag, k) {
    if (tag > 255) {
      throw Error('high-tag values not supported');
    }
    this.addU8_(tag);
    this.addLengthPrefixed_(1, true, k);
  }

  /**
   * Appends an ASN.1 INTEGER to the slice.
   * @param {number} n The value of the integer. Must be within the range of an
   *     int32.
   * @throws {Error}
   */
  addASN1Int(n) {
    if (n < (0x80000000 << 0) || n > 0x7fffffff) {
      // Numbers this large (or small) cannot be correctly shifted in
      // Javascript.
      throw Error('integer out of encodable range');
    }

    var length = 1;
    for (var nn = n; nn >= 0x80 || nn <= -0x80; nn >>= 8) {
      length++;
    }

    this.addASN1(Tag.INTEGER, (b) => {
      for (var i = length - 1; i >= 0; i--) {
        b.addU8_((n >> (8 * i)) & 0xff);
      }
    });
  }

  /**
   * Appends a non-negative ASN.1 INTEGER to the slice given its big-endian
   *     encoding. This can be useful when interacting with the WebCrypto API.
   * @param {!Uint8Array} bytes The big-endian encoding of the integer.
   * @throws {Error}
   */
  addASN1BigInt(bytes) {
    // Zero is representated as a single zero byte, rather than no bytes.
    if (bytes.length === 0) {
      bytes = new Uint8Array(1);
    }

    // Leading zero bytes need to be removed, unless that would make the number
    // negative.
    while (bytes.length >= 2 && bytes[0] === 0 && (bytes[1] & 0x80) === 0) {
      bytes = bytes.slice(1);
    }

    // If the MSB is set, the number will be considered to be negative. Thus
    // a zero prefix is needed in that case.
    if (bytes.length > 0 && (bytes[0] & 0x80) === 0x80) {
      if (bytes.length > Number.MAX_SAFE_INTEGER - 1) {
        throw Error('bigint array too long');
      }
      var newBytes = new Uint8Array(bytes.length + 1);
      newBytes.set(bytes, 1);
      bytes = newBytes;
    }

    this.addASN1(Tag.INTEGER, (b) => b.addBytes(bytes));
  }

  /**
   * Appends a base128-encoded integer to the slice.
   * @param {number} n The value of the integer. Must be non-negative and within
   *     the range of an int32.
   * @throws {Error}
   * @private
   */
  addBase128Int_(n) {
    if (n < 0 || n > 0x7fffffff) {
      // Cannot encode negative numbers and large numbers cannot be shifted in
      // Javascript.
      throw Error('integer out of encodable range');
    }

    var length = 0;
    if (n === 0) {
      length = 1;
    } else {
      for (var i = n; i > 0; i >>= 7) {
        length++;
      }
    }

    for (var i = length - 1; i >= 0; i--) {
      var octet = 0x7f & (n >> (7 * i));
      if (i !== 0) {
        octet |= 0x80;
      }
      this.addU8_(octet);
    }
  }

  /**
   * Appends an OBJECT IDENTIFIER to the slice.
   * @param {Array<number>} oid The OID as a list of integer elements.
   * @throws {Error}
   */
  addASN1ObjectIdentifier(oid) {
    if (oid.length < 2 || oid[0] > 2 || (oid[0] <= 1 && oid[1] >= 40)) {
      throw Error('invalid OID');
    }

    this.addASN1(Tag.OBJECT, (b) => {
      b.addBase128Int_(oid[0] * 40 + oid[1]);
      for (var i = 2; i < oid.length; i++) {
        b.addBase128Int_(oid[i]);
      }
    });
  }

  /**
   * Appends an ASN.1 NULL to the slice.
   * @throws {Error}
   */
  addASN1Null() {
    const offset = this.extend_(2);
    this.slice_[offset] = Tag.NULL;
    this.slice_[offset + 1] = 0;
  }

  /**
   * Appends an ASN.1 PrintableString to the slice.
   * @param {string} s The contents of the string.
   * @throws {Error}
   */
  addASN1PrintableString(s) {
    var buf = new Uint8Array(s.length);
    for (var i = 0; i < s.length; i++) {
      const code = s.charCodeAt(i);
      if ((code < 97 && code > 122) &&  // a-z
          (code < 65 && code > 90) &&   // A-Z
          ' \'()+,-/:=?'.indexOf(String.fromCharCode(code)) === -1) {
        throw Error(
            'cannot encode \'' + String.fromCharCode(code) + '\' in' +
            ' PrintableString');
      }

      buf[i] = code;
    }

    this.addASN1(Tag.PrintableString, (b) => {
      b.addBytes(buf);
    });
  }

  /**
   * Appends an ASN.1 UTF8String to the slice.
   * @param {string} s The contents of the string.
   * @throws {Error}
   */
  addASN1UTF8String(s) {
    this.addASN1(Tag.UTF8String, (b) => {
      b.addBytes((new TextEncoder()).encode(s));
    });
  }

  /**
   * Appends an ASN.1 BIT STRING to the slice.
   * @param {!Uint8Array} bytes The contents, which must be a whole number of
   *     bytes.
   * @throws {Error}
   */
  addASN1BitString(bytes) {
    this.addASN1(Tag.BITSTRING, (b) => {
      b.addU8_(0);  // no superfluous bits in encoding.
      b.addBytes(bytes);
    });
  }

  /**
   * Appends raw data to the slice.
   * @param {string} s The contents to append. All character values must
   *     be < 256.
   * @throws {Error}
   */
  addBytesFromString(s) {
    const buf = new Uint8Array(s.length);
    for (var i = 0; i < s.length; i++) {
      const code = s.charCodeAt(i);
      if (code > 255) {
        throw Error('out-of-range character in string of bytes');
      }
      buf[i] = code;
    }

    this.addBytes(buf);
  }

  /**
   * Appends raw bytes to the slice.
   * @param {!Array<number>|!Uint8Array} bytes Data to append.
   * @throws {Error}
   */
  addBytes(bytes) {
    const offset = this.extend_(bytes.length);
    for (var i = 0; i < bytes.length; i++) {
      this.slice_[offset + i] = bytes[i];
    }
  }
};
