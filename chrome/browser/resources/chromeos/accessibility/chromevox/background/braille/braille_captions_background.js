// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview If the braille captions feature is enabled, sends
 * braille content to the Panel on Chrome OS, or a content script on
 * other platforms.
 */
import {LocalStorage} from '../../../common/local_storage.js';
import {BrailleDisplayState} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {Msgs} from '../../common/msgs.js';
import {PanelCommand, PanelCommandType} from '../../common/panel_command.js';
import {QueueMode} from '../../common/tts_types.js';
import {ChromeVox} from '../chromevox.js';
import {ChromeVoxPrefs} from '../prefs.js';

export class BrailleCaptionsBackground {
  /**
   * @param {function()} stateCallback Called when the state of the captions
   *     feature changes.
   */
  constructor(stateCallback) {
    /** @private {function()} */
    this.stateCallback_ = stateCallback;
  }

  /**
   * Called once to initialize the class.
   * @param {function()} stateCallback Called when the state of the captions
   *     feature changes.
   */
  static init(stateCallback) {
    BrailleCaptionsBackground.instance =
        new BrailleCaptionsBackground(stateCallback);
  }

  /**
   * Returns whether the braille captions feature is enabled.
   * @return {boolean}
   */
  static isEnabled() {
    return LocalStorage.get(BrailleCaptionsBackground.PREF_KEY);
  }

  /**
   * @param {string} text Text of the shown braille.
   * @param {ArrayBuffer} cells Braille cells shown on the display.
   * @param {Array<number>} brailleToText Map of Braille letters to the first
   *     index of corresponding text letter.
   * @param {{brailleOffset: number, textOffset: number}} offsetsForSlices
   *    Offsets to use for calculating indices. The element is the braille
   * offset, the second is the text offset.
   * @param {number} rows Number of rows to display.
   * @param {number} columns Number of columns to display.
   */
  static setContent(
      text, cells, brailleToText, offsetsForSlices, rows, columns) {
    // Convert the cells to Unicode braille pattern characters.
    const byteBuf = new Uint8Array(cells);
    let brailleChars = '';

    for (let i = 0; i < byteBuf.length; ++i) {
      brailleChars += String.fromCharCode(
          BrailleCaptionsBackground.BRAILLE_UNICODE_BLOCK_START | byteBuf[i]);
    }
    const groups = BrailleCaptionsBackground.groupBrailleAndText(
        brailleChars, text, brailleToText, offsetsForSlices);
    const data = {groups, rows, cols: columns};
    (new PanelCommand(PanelCommandType.UPDATE_BRAILLE, data)).send();
  }

  /**
   * @param {ArrayBuffer} cells Braille cells shown on the display.
   * @param {number} rows Number of rows to display.
   * @param {number} columns Number of columns to display.
   */
  static setImageContent(cells, rows, columns) {
    // Convert the cells to Unicode braille pattern characters.
    const byteBuf = new Uint8Array(cells);
    let brailleChars = '';

    for (let i = 0; i < byteBuf.length; ++i) {
      brailleChars += String.fromCharCode(
          BrailleCaptionsBackground.BRAILLE_UNICODE_BLOCK_START | byteBuf[i]);
    }

    const groups = [['Image', brailleChars]];
    const data = {groups, rows, cols: columns};
    (new PanelCommand(PanelCommandType.UPDATE_BRAILLE, data)).send();
  }

  /**
   * @param {string} brailleChars Braille characters shown on the display.
   * @param {string} text Text of the shown braille.
   * @param {Array<number>} brailleToText Map of Braille cells to the first
   *     index of corresponding text letter.
   * @param {{brailleOffset: number, textOffset: number}} offsets
   *    Offsets to use for calculating indices. The element is the braille
   * offset, the second is the text offset.
   * @return {Array} The groups of braille and texts to be displayed.
   */
  static groupBrailleAndText(brailleChars, text, brailleToText, offsets) {
    let brailleBuf = '';
    const groups = [];
    let textIndex = 0;
    const brailleOffset = offsets.brailleOffset;
    const textOffset = offsets.textOffset;
    const calculateTextIndex = index =>
        brailleToText[index + brailleOffset] - textOffset;

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
  }

  /**
   * Sets whether the overlay should be active.
   * @param {boolean} newValue The new value of the active flag.
   */
  static setActive(newValue) {
    const oldValue = BrailleCaptionsBackground.isEnabled();
    ChromeVoxPrefs.instance.setPref(
        BrailleCaptionsBackground.PREF_KEY, newValue);
    if (oldValue !== newValue) {
      BrailleCaptionsBackground.instance.callStateCallback_();
      const msg = newValue ? Msgs.getMsg('braille_captions_enabled') :
                             Msgs.getMsg('braille_captions_disabled');
      ChromeVox.tts.speak(msg, QueueMode.QUEUE);
      ChromeVox.braille.write(NavBraille.fromText(msg));
    }
  }

  /**
   * Asynchronously returns a display state representing the state of the
   * captions feature. This is used when no actual hardware display is
   * connected.
   * @return {!Promise<!BrailleDisplayState>}
   */
  static async getVirtualDisplayState() {
    return new Promise(async resolve => {
      if (BrailleCaptionsBackground.isEnabled()) {
        let items = await new Promise(
            resolve =>
                chrome.storage.local.get({'virtualBrailleRows': 1}, resolve));
        const rows = items['virtualBrailleRows'];
        items = await new Promise(
            resolve => chrome.storage.local.get(
                {'virtualBrailleColumns': 40}, resolve));
        const columns = items['virtualBrailleColumns'];
        resolve(
            {available: true, textRowCount: rows, textColumnCount: columns});
      } else {
        resolve({available: false, textRowCount: 0, textColumnCount: 0});
      }
    });
  }

  /** @private */
  callStateCallback_() {
    if (this.stateCallback_) {
      this.stateCallback_();
    }
  }
}

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
