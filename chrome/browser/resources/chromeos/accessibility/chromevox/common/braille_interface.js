// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a Braille interface.
 *
 * All Braille engines in ChromeVox conform to this interface.
 *
 */

goog.provide('BrailleInterface');

goog.require('BrailleKeyCommand');
goog.require('BrailleKeyEvent');
goog.require('NavBraille');

/** @interface */
BrailleInterface = class {
  constructor() {}

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
};
