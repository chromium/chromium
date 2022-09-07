// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

  /**
   * @fileoverview
   * 'CrPngBehavior' is a behavior to convert image sequences into APNG
   * (animated PNG) images.
   */

  /**
   * PNG frame delay fraction numerator.
   * @const
   */
  const PNG_FRAME_DELAY_NUMERATOR = 1;

  /**
   * PNG frame delay fraction denominator.
   * @const
   */
  const PNG_FRAME_DELAY_DENOMINATOR = 20;

  /**
   * PNG signature.
   * @const
   */
  const PNG_SIGNATURE = [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A];

  /**
   * PNG bit depth (8 = 32bpp).
   * @const
   */
  const PNG_BIT_DEPTH = 8;

  /**
   * PNG compression method (0 = deflate/inflate compression with a sliding
   * window PNG compression).
   * @const
   */
  const PNG_COMPRESSION_METHOD = 0;

  /**
   * PNG filter method (0 = adaptive filtering with five basic filter types).
   * @const
   */
  const PNG_FILTER_METHOD = 0;

  /**
   * PNG interlace method (0 = no interlace).
   * @const
   */
  const PNG_INTERLACE_METHOD = 0;

  /**
   * CRC table for PNG encode.
   *
   * Generated using:
   *
   * for (var i = 0; i < 256; i++) {
   *   var value = i;
   *   for (var j = 0; j < 8; j++) {
   *     if (value & 1)
   *       value = ((0xEDB88320) ^ (value >>> 1));
   *     else
   *       value = (value >>> 1);
   *   }
   *   TABLE[i] = value;
   * }
   *
   * @const
   */
  const PNG_CRC_TABLE = [
    0x0,        0x77073096, 0xEE0E612C, 0x990951BA, 0x76DC419,  0x706AF48F,
    0xE963A535, 0x9E6495A3, 0xEDB8832,  0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x9B64C2B,  0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x1DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x6B6B51F,  0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0xF00F934,  0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x86D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x3B6E20C,  0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x4DB2615,  0x73DC1683, 0xE3630B12, 0x94643B84,
    0xD6D6A3E,  0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0xA00AE27,  0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x26D930A,  0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x5005713,  0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0xCB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0xBDBDF21,  0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
  ];

  /**
   * PNG object state.
   * @typedef {{
   *   frames: number,
   *   sequences: number,
   *   width: number,
   *   height: number,
   *   colour: number,
   *   chunks: !Array<Uint8Array>
   * }}
   */
  let CrPngState;

  /**
   * Construct an internal representation of the png.
   * @param {!Array<string>} images The data URLs for each image.
   * @return {!CrPngState}
   * @private
   */
  function convertDataUrlsToCrPng(images) {
    const png =
        /** @type {!CrPngState} */ ({frames: 0, sequences: 0, chunks: []});

    /** Append signature. */
    png.chunks.push(new Uint8Array(PNG_SIGNATURE));

    /**
     * http://www.w3.org/TR/2003/REC-PNG-20031110/#11IHDR
     *
     * Width               4 bytes
     * Height              4 bytes
     * Bit depth           1 byte
     * Colour type         1 byte
     * Compression method  1 byte
     * Filter method       1 byte
     * Interlace method    1 byte
     */
    const IHDR = new Uint8Array(12 + 13);
    writeUInt32(IHDR, 13, 0);
    writeFourCC(IHDR, 'IHDR', 4);
    /** Write size at the end when known. */
    writeUInt8(IHDR, PNG_BIT_DEPTH, 16);
    /** Write colour at the end when known. */
    writeUInt8(IHDR, PNG_COMPRESSION_METHOD, 18);
    writeUInt8(IHDR, PNG_FILTER_METHOD, 19);
    writeUInt8(IHDR, PNG_INTERLACE_METHOD, 20);
    /** Write CRC at the end when size and colour is known. */
    png.chunks.push(IHDR);

    /**
     * acTL
     *
     * Number of frames         4 bytes
     * Number of times to loop  4 bytes
     */
    const acTL = new Uint8Array(12 + 8);
    writeUInt32(acTL, 8, 0);
    writeFourCC(acTL, 'acTL', 4);
    writeUInt32(acTL, images.length, 8);
    writeUInt32(acTL, 0, 12);
    writeUInt32(acTL, getCRC(acTL, 4, 16), 16);
    png.chunks.push(acTL);

    /** Append each image as a PNG frame. */
    for (let i = 0; i < images.length; ++i) {
      appendFrameFromDataURL(images[i], png);
    }

    /** Update IHDR now that size and colour is known. */
    writeUInt32(IHDR, png.width, 8);
    writeUInt32(IHDR, png.height, 12);
    writeUInt8(IHDR, png.colour, 17);
    writeUInt32(IHDR, getCRC(IHDR, 4, 8 + 13), 8 + 13);

    /**
     * http://www.w3.org/TR/2003/REC-PNG-20031110/#11IEND
     */
    const IEND = new Uint8Array(12);
    writeUInt32(IEND, 0, 0);
    writeFourCC(IEND, 'IEND', 4);
    writeUInt32(IEND, getCRC(IEND, 4, 8), 8);
    png.chunks.push(IEND);

    return png;
  }

  /**
   * Get a binary representation of the png.
   * @param {!Array<string>} images The data URLs for each image.
   * @return {!Uint8Array} Binary data for animated png image.
   */
  export function convertImageSequenceToPngBinary(images) {
    const png = convertDataUrlsToCrPng(images);
    const numBytes =
        png.chunks.reduce((value, next) => value + next.byteLength, 0);
    const result = new Uint8Array(numBytes);
    let offset = 0;
    for (const chunk of png.chunks) {
      result.set(/** @type {!ArrayBufferView} */ (chunk), offset);
      offset += chunk.byteLength;
    }
    return result;
  }

  /**
   * Returns a data URL for an animated PNG image that is created
   * from a sequence of images.
   * @param {!Array<string>} images The data URLs for each image.
   * @return {string} A data URL for an animated PNG image.
   */
  export function convertImageSequenceToPng(images) {
    const png = convertDataUrlsToCrPng(images);
    return 'data:image/png;base64,' +
        btoa(png.chunks
                 .map(function(chunk) {
                   return String.fromCharCode.apply(null, chunk);
                 })
                 .join(''));
  }

  /**
   * Returns true if the data URL is an animated PNG image.  If the PNG is
   * animated, then 'acTL' will have been set by convertImageSequenceToPng().
   * acTL is the animation control chunk in the data stream of an animated PNG.
   * If it exists we assume the image is animated, regardless of the number of
   * frames. The offset is the PNG signature ( 8 bytes) + IHDR (25 bytes) + 4
   * bytes of padding zeros. See https://wiki.mozilla.org/APNG_Specification
   * @param {string} url An btoa encoded data URL for a PNG image.
   * @return {boolean} True if data URL is an animated PNG image.
   */
  export function isEncodedPngDataUrlAnimated(url) {
    const decoded = atob(url.substr('data:image/png;base64,'.length));
    return decoded.substr(37, 4) === 'acTL';
  }

  /**
   * Reads Uint32 from buffer.
   * @param {!Uint8Array} buffer Buffer to read UInt32 from.
   * @param {number} offset Offset in buffer to read UInt32 at.
   * @return {number} The value read.
   */
  function readUInt32(buffer, offset) {
    return (buffer[offset + 0] << 24) + (buffer[offset + 1] << 16) +
        (buffer[offset + 2] << 8) + (buffer[offset + 3] << 0);
  }

  /**
   * Reads string from buffer.
   * @param {!Uint8Array} buffer Buffer to read string from.
   * @param {number} offset Offset in buffer to read string at.
   * @param {number} length Length of string to read.
   * @return {string} The value read.
   */
  function readString(buffer, offset, length) {
    let str = '';
    for (let i = 0; i < length; i++) {
      str += String.fromCharCode(buffer[offset + i]);
    }
    return str;
  }

  /**
   * Write bytes to buffer.
   * @param {!Uint8Array} buffer Buffer to write bytes to.
   * @param {!Uint8Array} bytes Array of bytes to be written.
   * @param {number} offset Offset in buffer to write bytes at.
   */
  function writeBytes(buffer, bytes, offset) {
    for (let i = 0; i < bytes.length; i++) {
      buffer[offset + i] = bytes[i] & 0xFF;
    }
  }

  /**
   * Write UInt8 to buffer.
   * @param {!Uint8Array} buffer Buffer to write UInt8 to.
   * @param {number} u8 UInt8 to be written.
   * @param {number} offset Offset in buffer to write UInt8 at.
   */
  function writeUInt8(buffer, u8, offset) {
    buffer[offset] = u8 & 0xFF;
  }

  /**
   * Write UInt16 to buffer.
   * @param {!Uint8Array} buffer Buffer to write UInt16 to.
   * @param {number} u16 UInt16 to be written.
   * @param {number} offset Offset in buffer to write UInt16 at.
   */
  function writeUInt16(buffer, u16, offset) {
    buffer[offset + 0] = (u16 >> 8) & 0xFF;
    buffer[offset + 1] = (u16 >> 0) & 0xFF;
  }

  /**
   * Write UInt32 to buffer.
   * @param {!Uint8Array} buffer Buffer to write UInt32 to.
   * @param {number} u32 UInt32 to be written.
   * @param {number} offset Offset in buffer to write UInt32 at.
   */
  function writeUInt32(buffer, u32, offset) {
    buffer[offset + 0] = (u32 >> 24) & 0xFF;
    buffer[offset + 1] = (u32 >> 16) & 0xFF;
    buffer[offset + 2] = (u32 >> 8) & 0xFF;
    buffer[offset + 3] = (u32 >> 0) & 0xFF;
  }

  /**
   * Write string to buffer.
   * @param {!Uint8Array} buffer Buffer to write string to.
   * @param {string} string String to be written.
   * @param {number} offset Offset in buffer to write string at.
   */
  function writeString(buffer, string, offset) {
    for (let i = 0; i < string.length; i++) {
      buffer[offset + i] = string.charCodeAt(i);
    }
  }

  /**
   * Write FourCC code to buffer.
   * @param {!Uint8Array} buffer Buffer to write FourCC code to.
   * @param {string} fourcc FourCC code to be written.
   * @param {number} offset Offset in buffer to write FourCC code at.
   */
  function writeFourCC(buffer, fourcc, offset) {
    buffer[offset + 0] = fourcc.charCodeAt(0);
    buffer[offset + 1] = fourcc.charCodeAt(1);
    buffer[offset + 2] = fourcc.charCodeAt(2);
    buffer[offset + 3] = fourcc.charCodeAt(3);
  }

  /**
   * Compute CRC from buffer data.
   * @param {!Uint8Array} buffer Buffer with data to compute CRC from.
   * @param {number} start Start index in buffer.
   * @param {number} end End index in buffer.
   * @return {number} The computed CRC.
   */
  function getCRC(buffer, start, end) {
    let crc = 0xFFFFFFFF;
    for (let i = start; i < end; i++) {
      const crcTableIndex = (crc ^ (buffer[i])) & 0xFF;
      crc = PNG_CRC_TABLE[crcTableIndex] ^ (crc >>> 8);
    }
    return crc ^ 0xFFFFFFFF;
  }

  /**
   * Append frame from data URL to PNG object.
   * @param {string} dataURL Data URL for frame.
   * @param {!CrPngState} png PNG object to add frame to.
   */
  function appendFrameFromDataURL(dataURL, png) {
    /** Convert data URL to Uint8Array. */
    const byteString = atob(dataURL.split(',')[1]);
    const bytes = new Uint8Array(byteString.length);
    writeString(bytes, byteString, 0);

    /** Check signature. */
    const signature = bytes.subarray(0, PNG_SIGNATURE.length);
    if (signature.toString() !== PNG_SIGNATURE.toString()) {
      console.error('Bad PNG signature');
    }

    /**
     * fcTL
     *
     * Sequence number          4 bytes
     * Width                    4 bytes
     * Height                   4 bytes
     * X position               4 bytes
     * Y position               4 bytes
     * Frame delay numerator    2 bytes
     * Frame delay denominator  2 bytes
     * Dispose op               1 bytes
     * Blend op                 1 bytes
     */
    const fcTL = new Uint8Array(12 + 26);
    writeUInt32(fcTL, 26, 0);
    writeFourCC(fcTL, 'fcTL', 4);
    writeUInt32(fcTL, png.sequences, 8);
    /** Write size at the end when known. */
    writeUInt32(fcTL, 0, 20);
    writeUInt32(fcTL, 0, 24);
    writeUInt16(fcTL, PNG_FRAME_DELAY_NUMERATOR, 28);
    writeUInt16(fcTL, PNG_FRAME_DELAY_DENOMINATOR, 30);
    writeUInt8(fcTL, 0, 32);
    writeUInt8(fcTL, 0, 33);
    /** Write CRC at the end when size is known. */
    png.sequences += 1;
    png.chunks.push(fcTL);

    /** Append data chunks for frame. */
    let i = PNG_SIGNATURE.length;
    while ((i + 12) <= bytes.length) {
      /**
       * http://www.w3.org/TR/2003/REC-PNG-20031110/#5Chunk-layout
       *
       * length =  4      bytes
       * type   =  4      bytes (IHDR, PLTE, IDAT, IEND or others)
       * chunk  =  length bytes
       * crc    =  4      bytes
       */
      const length = readUInt32(bytes, i);
      const type = readString(bytes, i + 4, 4);
      const chunk = bytes.subarray(i + 8, i + 8 + length);

      /** We should have enough bytes left for length. */
      if (length !== chunk.length) {
        console.error('Unexpectedly reached end of file');
      }

      switch (type) {
        case 'IHDR':
          /**
           * http://www.w3.org/TR/2003/REC-PNG-20031110/#11IHDR
           *
           * Width               4 bytes
           * Height              4 bytes
           * Bit depth           1 byte
           * Colour type         1 byte
           * Compression method  1 byte
           * Filter method       1 byte
           * Interlace method    1 byte
           */
          const width = readUInt32(chunk, 0);
          const height = readUInt32(chunk, 4);
          const depth = chunk[8];
          const colour = chunk[9];
          const compression = chunk[10];
          const filter = chunk[11];
          const interlace = chunk[12];

          /** Initialize size and colour if this is the first frame. */
          if (png.frames === 0) {
            png.width = width;
            png.height = height;
            png.colour = colour;
          }

          /** Check that header matches our expectations. */
          if (width !== png.width) {
            console.error('Bad PNG width: ' + width);
          }
          if (height !== png.height) {
            console.error('Bad PNG height: ' + height);
          }
          if (depth !== PNG_BIT_DEPTH) {
            console.error('Bad PNG bit depth: ' + depth);
          }
          if (colour !== png.colour) {
            console.error('Bad PNG colour type: ' + colour);
          }
          if (compression !== PNG_COMPRESSION_METHOD) {
            console.error('Bad PNG compression method: ' + compression);
          }
          if (filter !== PNG_FILTER_METHOD) {
            console.error('Bad PNG filter method: ' + filter);
          }
          if (interlace !== PNG_INTERLACE_METHOD) {
            console.error('Bad PNG interlace method: ' + interlace);
          }
          break;
        case 'IDAT':
          /** Append as IDAT chunk if this is the first frame. */
          if (png.frames === 0) {
            /**
             * http://www.w3.org/TR/2003/REC-PNG-20031110/#11IDAT
             *
             * Data                     X bytes
             */
            const IDAT = new Uint8Array(12 + length);
            writeUInt32(IDAT, length, 0);
            writeFourCC(IDAT, 'IDAT', 4);
            writeBytes(IDAT, chunk, 8);
            writeUInt32(IDAT, getCRC(IDAT, 4, 8 + length), 8 + length);
            png.chunks.push(IDAT);
          } else {
            /**
             * fdAT
             *
             * Sequence number          4 bytes
             * Frame data               X bytes
             */
            const fdAT = new Uint8Array(12 + 4 + length);
            writeUInt32(fdAT, 4 + length, 0);
            writeFourCC(fdAT, 'fdAT', 4);
            writeUInt32(fdAT, png.sequences, 8);
            writeBytes(fdAT, chunk, 12);
            writeUInt32(fdAT, getCRC(fdAT, 4, 12 + length), 12 + length);
            png.sequences += 1;
            png.chunks.push(fdAT);
          }
          break;
        case 'PLTE':
          /**
           * https://www.w3.org/TR/2003/REC-PNG-20031110/#11PLTE
           *
           * Palette data        X bytes
           */
          const PLTE = new Uint8Array(12 + length);
          writeUInt32(PLTE, length, 0);
          writeFourCC(PLTE, 'PLTE', 4);
          writeBytes(PLTE, chunk, 8);
          writeUInt32(PLTE, getCRC(PLTE, 4, 8 + length), 8 + length);
          png.chunks.push(PLTE);
          break;
        case 'IEND':
          /** Update fcTL now that size is known. */
          writeUInt32(fcTL, png.width, 12);
          writeUInt32(fcTL, png.height, 16);
          writeUInt32(fcTL, getCRC(fcTL, 4, 34), 34);
          png.frames += 1;
          return;
      }

      /** Advance to next chunk. */
      i += 12 + length;
    }
    console.error('Unexpectedly reached end of file');
  }
