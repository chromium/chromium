// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PIEX_LOADER_TEST_ONLY, PiexLoader, PiexWasmModule} from './piex_loader.js';

/**
 * Set when PiexLoader has an unrecoverable error to disable future attempts.
 */
let piexEnabled = true;

/** Handles wasm load failures. */
function onPiexModuleFailed() {
  piexEnabled = false;
}

const SIZEOF_APP1_PREFIX = 8;
const SIZEOF_TIFF_HEADER = 8;
const SIZEOF_SINGLE_IFD_FRAME = 18;
const NUM_IFD_FRAMES = 1;

/**
 * For the minimal header, just dump an orientation tag into the "0th" frame and
 * nothing else. There _should_ be other stuff, like a pointer to the "EXIF" IFD
 * frame (containing ExifVersion, etc.), and tags for image resolution. But
 * these are not used by Chrome for rendering.
 */
const SIZEOF_APP1_HEADER = SIZEOF_APP1_PREFIX + SIZEOF_TIFF_HEADER +
    NUM_IFD_FRAMES * SIZEOF_SINGLE_IFD_FRAME;

/**
 * A minimal EXIF header to include an orientation field. Includes the "Start of
 * image" prefix that should already be present, so that this can just form the
 * start of the resulting JPEG; with the quantization table following.
 *
 * The table at https://www.exif.org/Exif2-2.PDF#page=70 was the starting point
 * for this, but details come from all over the document.
 *
 * | Offset | Code |    Meaning      |
 * | ------ | ---- | --------------- |
 * |   -2   | 0xFF | SOI Prefix      |
 * |   -1   | 0xD8 | Start of Image  |
 * |   +0   | 0xFF | Marker Prefix   |
 * |   +1   | 0xE1 | APP1            |
 * |   +2   | 0x.. | Length[1]       |  // Always big-endian.
 * |   +3   | 0x.. | Length[0]       |  // SIZEOF_APP1_HEADER.
 * |   +4   | 0x45 | 'E'             |
 * |   +5   | 0x78 | 'x'             |
 * |   +6   | 0x69 | 'i'             |
 * |   +7   | 0x66 | 'f'             |
 * |   +8   | 0x00 | NULL            |
 * |   +9   | 0x00 | Padding         |
 * |        |      | TIFF header     |  // 8 bytes. Offsets start from here.
 * |   +10  | 0x49 | ByteOrder ('I') |  // Little-endian (seems more common).
 * |   +11  | 0x49 | ByteOrder ('I') |
 * |   +12  | 0x2a | IFD marker[0]   |
 * |   +13  | 0x00 | IFD marker[1]   |
 * |   +14  | 0x08 | IFD offset[0]   |  // 8 (0th IFD immediately follows).
 * |   +15  | 0x00 | IFD offset[1]   |
 * |   +16  | 0x00 | IFD offset[2]   |
 * |   +17  | 0x00 | IFD offset[3]   |
 * |        |      | IFD frame       |  // 18 bytes
 * |   +18  | 0x01 | Field count[0]  |  // 1 Field (just orientation).
 * |   +19  | 0x00 | Field count[1]  |
 * |   +20  | 0x12 | TAG[0]          |  // E.g. TAG_ORIENTATION (0x0112)
 * |   +21  | 0x01 | TAG[1]          |
 * |   +22  | 0x03 | Data type[0]    |  // 3 = "Short"
 * |   +23  | 0x00 | Data type[1]    |
 * |   +24  | 0x01 | Data count[0]   |  // 1
 * |   +25  | 0x00 | Data count[1]   |
 * |   +26  | 0x00 | Data count[2]   |
 * |   +27  | 0x00 | Data count[3]   |
 * | 28-29  | 0x.. | Value           |  // Orientation goes here! <omitted>
 * | 30-31  | 0x00 | Padding         |
 * | 32-35  | 0x00 | Offset to next  |  // 0 to indicate "no more".
 */
const TIFF_HEADER = new Uint8Array([
  0xff,
  0xd8,
  0xff,
  0xe1,
  SIZEOF_APP1_HEADER >> 8,
  SIZEOF_APP1_HEADER & 0xff,
  0x45,
  0x78,
  0x69,
  0x66,
  0x00,
  0x00,
  0x49,
  0x49,
  0x2a,
  0x00,
  0x08,
  0x00,
  0x00,
  0x00,
]);

/**
 * Makes an 18-byte little-endian IFD frame for the Exif orientation value.
 * This is a number [1, 8]. The Exif spec requests a 16-bit unsigned int to be
 * written.
 * Reference: https://www.exif.org/Exif2-2.PDF#page=19.
 */
function makeOrientationIfdFrame(value: number) {
  const LITTLE_ENDIAN = true;
  const TAG_ORIENTATION = 0x0112;
  const TYPE_UINT16 = 3;   // 1=BYTE, 2=ASCII, 3=SHORT, 4=LONG, etc.
  const FIELD_COUNT = 1;   // Just writing orientation.
  const VALUE_LENGTH = 1;  // Writing one "short".
  const NEXT_OFFSET = 0;   // No more frames.
  const buffer = new ArrayBuffer(SIZEOF_SINGLE_IFD_FRAME);
  const view = new DataView(buffer);
  view.setUint16(0, FIELD_COUNT, LITTLE_ENDIAN);
  view.setUint16(2, TAG_ORIENTATION, LITTLE_ENDIAN);
  view.setUint16(4, TYPE_UINT16, LITTLE_ENDIAN);
  view.setUint32(6, VALUE_LENGTH, LITTLE_ENDIAN);

  // The value is <= 4 bytes, so write directly. Otherwise an offset to the
  // value data would be written here. Note the type "hugs" the low-order bits
  // and is followed by padding if the data size is < 4 bytes.
  view.setUint16(10, value, LITTLE_ENDIAN);

  view.setUint32(14, NEXT_OFFSET, LITTLE_ENDIAN);
  return buffer;
}

/**
 * Extracts a JPEG from a RAW Image ArrayBuffer.
 */
async function extractFromRawImageBuffer(buffer: ArrayBuffer) {
  /** Application Segment Marker. */
  const APP1_MARKER = 0xffe1;

  /** SOI. Page 64. */
  const START_OF_IMAGE = 0xffd8;

  /** Field value for no rotation. Don't add an Exif header in this case. */
  const NO_ROTATION = 1;

  if (!piexEnabled) {
    throw new Error('Piex disabled');
  }
  const response = await PiexLoader.load(buffer, onPiexModuleFailed);
  // Note the "thumbnail" is usually the full-sized image "preview", but may
  // fall back to a thumbnail when that is unavailable.
  const jpegData = response.thumbnail;

  function original(warning = '') {
    if (warning) {
      console.warn(`Returning unrotated image: ${warning}.`);
    }
    return new File([jpegData], 'raw-preview', {type: response.mimeType});
  }

  if (response.orientation === NO_ROTATION) {
    return original();
  }

  const view = new DataView(jpegData);
  if (view.getUint16(0) !== START_OF_IMAGE) {
    return original('No SOI');
  }

  // Files returned by Piex should begin immediately with JPEG headers (and the
  // Define Quantization Table marker). If a layer between here and Piex has
  // added its own APP marker segment(s), don't add a duplicate.
  if (view.getUint16(2) === APP1_MARKER) {
    return original('APP1 marker already present');
  }

  // Ignore the Start-Of-Image already in `jpegData` (TIFF_HEADER has one).
  const jpegWithoutSOI = (new Blob([jpegData])).slice(2);

  const orientation = makeOrientationIfdFrame(response.orientation);

  return new File(
      [TIFF_HEADER, orientation, jpegWithoutSOI], 'raw-preview',
      {type: response.mimeType});
}

declare global {
  interface Window {
    getPiexModuleForTesting: () => PiexWasmModule;
    extractFromRawImageBuffer: (buffer: ArrayBuffer) => Promise<File>;
  }
}

// Export to `window` manually until the toolchain has better support for
// dynamic module loading. Dynamic modules require ES2020, but asking chromium's
// closure typechecking toolchain for ES2020 input and output still complains
// that dynamic imports can't be transpiled.
window.extractFromRawImageBuffer = extractFromRawImageBuffer;

// Expose the module on `window` for MediaAppIntegrationTest.HandleRawFiles.
// TODO(b/185957537): Convert the test case to a JS module.
window.getPiexModuleForTesting = PIEX_LOADER_TEST_ONLY.getModule;
