// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a Braille interface.
 * All Braille engines in ChromeVox conform to this interface.
 */

import {BrailleDisplayState} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';

/** @interface */
export class BrailleInterface {
  /**
   * Sends the given params to the Braille display for output.
   * @param {!NavBraille} params Parameters to send to the
   * platform braille service.
   */
  write(params) {}

  /**
   * Takes an image in the form of a data url and outputs it to a Braille
   * display.
   * @param {!string} imageDataUrl The image to output, in the form of a
   * dataUrl.
   */
  writeRawImage(imageDataUrl) {}

  /**
   * Freeze whatever is on the braille display until the next call to thaw().
   */
  freeze() {}

  /**
   * Un-freeze the braille display so that it can be written to again.
   */
  thaw() {}

  /**
   * @return {!BrailleDisplayState} The current display state.
   */
  getDisplayState() {}

  /**
   * Requests the braille display pan left.
   */
  panLeft() {}

  /**
   * Requests the braille display pan right.
   */
  panRight() {}

  /**
   * Moves the cursor to the given braille position.
   * @param {number|undefined} braillePosition The 0-based position relative to
   *     the start of the currently displayed text. The position is given in
   *     braille cells, not text cells.
   */
  route(braillePosition) {}

  /**
   * Translate braille cells into text.
   * @param {!ArrayBuffer} cells Cells to be translated.
   * @return {!Promise<?string>}
   */
  async backTranslate(cells) {}
}
