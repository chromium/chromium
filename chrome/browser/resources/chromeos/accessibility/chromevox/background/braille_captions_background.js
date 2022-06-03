// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview If the braille captions feature is enabled, sends
 * braille content to the Panel on Chrome OS, or a content script on
 * other platforms.
 */

goog.provide('BrailleCaptionsBackground');

goog.require('PanelCommand');
goog.require('BrailleDisplayState');
goog.require('ExtensionBridge');

/**
 * Key set in local storage when this feature is enabled.
 * @const
 */
BrailleCaptionsBackground.PREF_KEY = 'brailleCaptions';


/**
 * Unicode block of braille pattern characters.  A braille pattern is formed
 * from this value with the low order 8 bits set to the bits representing
 * the dots as per the ISO 11548-1 standard.
 * @const
 */
BrailleCaptionsBackground.BRAILLE_UNICODE_BLOCK_START = 0x2800;


/**
 * Called once to initialize the class.
 * @param {function()} stateCallback Called when the state of the captions
 *     feature changes.
 */
BrailleCaptionsBackground.init = function(stateCallback) {
  const self = BrailleCaptionsBackground;
  /**
   * @type {function()}
   * @private
   */
  self.stateCallback_ = stateCallback;
};


/**
 * Returns whether the braille captions feature is enabled.
 * @return {boolean}
 */
BrailleCaptionsBackground.isEnabled = function() {
  const self = BrailleCaptionsBackground;
  return localStorage[self.PREF_KEY] === String(true);
};

/**
 * @param {string} text Text of the shown braille.
 * @param {ArrayBuffer} cells Braille cells shown on the display.
 * @param {Array<number>} brailleToText Map of Braille letters to the first
 *     index of corresponding text letter.
 * @param {{brailleOffset: number, textOffset: number}} offsetsForSlices
 *    Offsets to use for caculating indices. The element is the braille offset,
 *    the second is the text offset.
 * @param {number} rows Number of rows to display.
 * @param {number} columns Number of columns to display.
 */
BrailleCaptionsBackground.setContent = function(
    text, cells, brailleToText, offsetsForSlices, rows, columns) {
  const self = BrailleCaptionsBackground;
  // Convert the cells to Unicode braille pattern characters.
  const byteBuf = new Uint8Array(cells);
  let brailleChars = '';

  for (let i = 0; i < byteBuf.length; ++i) {
    brailleChars +=
        String.fromCharCode(self.BRAILLE_UNICODE_BLOCK_START | byteBuf[i]);
  }
  const groups = BrailleCaptionsBackground.groupBrailleAndText(
      brailleChars, text, brailleToText, offsetsForSlices);
  const data = {groups, rows, cols: columns};
  (new PanelCommand(PanelCommandType.UPDATE_BRAILLE, data)).send();
};

/**
 * @param {ArrayBuffer} cells Braille cells shown on the display.
 * @param {number} rows Number of rows to display.
 * @param {number} columns Number of columns to display.
 */
BrailleCaptionsBackground.setImageContent = function(cells, rows, columns) {
  const self = BrailleCaptionsBackground;
  // Convert the cells to Unicode braille pattern characters.
  const byteBuf = new Uint8Array(cells);
  let brailleChars = '';

  for (let i = 0; i < byteBuf.length; ++i) {
    brailleChars +=
        String.fromCharCode(self.BRAILLE_UNICODE_BLOCK_START | byteBuf[i]);
  }

  const groups = [['Image', brailleChars]];
  const data = {groups, rows, cols: columns};
  (new PanelCommand(PanelCommandType.UPDATE_BRAILLE, data)).send();
};

/**
 * @param {string} brailleChars Braille characters shown on the display.
 * @param {string} text Text of the shown braille.
 * @param {Array<number>} brailleToText Map of Braille cells to the first
 *     index of corresponding text letter.
 * @param {{brailleOffset: number, textOffset: number}} offsets
 *    Offsets to use for caculating indices. The element is the braille offset,
 *    the second is the text offset.
 * @return {Array} The groups of braille and texts to be displayed.
 */
BrailleCaptionsBackground.groupBrailleAndText = function(
    brailleChars, text, brailleToText, offsets) {
  let brailleBuf = '';
  const groups = [];
  let textIndex = 0;
  const brailleOffset = offsets.brailleOffset;
  const textOffset = offsets.textOffset;
  const calculateTextIndex = function(index) {
    return brailleToText[index + brailleOffset] - textOffset;
  };

  for (let i = 0; i < brailleChars.length; ++i) {
    const textSliceIndex = calculateTextIndex(i);
    if (i !== 0 && textSliceIndex !== textIndex) {
      groups.push(
          [text.substr(textIndex, textSliceIndex - textIndex), brailleBuf]);
      brailleBuf = '';
      textIndex = textSliceIndex;
    }
    brailleBuf += brailleChars.charAt(i);
  }

  // Puts the rest of the text into the last group.
  if (brailleBuf.length > 0 && text.charAt(textIndex) !== ' ') {
    groups.push([text.substr(textIndex), brailleBuf]);
  }
  return groups;
};

/**
 * Sets whether the overlay should be active.
 * @param {boolean} newValue The new value of the active flag.
 */
BrailleCaptionsBackground.setActive = function(newValue) {
  const self = BrailleCaptionsBackground;
  const oldValue = self.isEnabled();
  window['prefs'].setPref(self.PREF_KEY, String(newValue));
  if (oldValue !== newValue) {
    if (self.stateCallback_) {
      self.stateCallback_();
    }
    const msg = newValue ? Msgs.getMsg('braille_captions_enabled') :
                           Msgs.getMsg('braille_captions_disabled');
    ChromeVox.tts.speak(msg, QueueMode.QUEUE);
    ChromeVox.braille.write(NavBraille.fromText(msg));
  }
};

/**
 * Calls a callback on a display state representing the state of the captions
 * feature. This is used when no actual hardware display is connected.
 * @param {function(!BrailleDisplayState)} callback The callback to pass
 * the display state into.
 */
BrailleCaptionsBackground.getVirtualDisplayState = function(callback) {
  const self = BrailleCaptionsBackground;
  if (self.isEnabled()) {
    chrome.storage.local.get({'virtualBrailleRows': 1}, function(items) {
      const rows = items['virtualBrailleRows'];
      chrome.storage.local.get({'virtualBrailleColumns': 40}, function(items) {
        const columns = items['virtualBrailleColumns'];
        callback(
            {available: true, textRowCount: rows, textColumnCount: columns});
      });
    });
  } else {
    callback({available: false, textRowCount: 0, textColumnCount: 0});
  }
};
