// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview If the braille captions feature is enabled, sends
 * braille content to the Panel on Chrome OS, or a content script on
 * other platforms.
 */
import {LocalStorage} from '/common/local_storage.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BrailleDisplayState} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {Msgs} from '../../common/msgs.js';
import {PanelCommand, PanelCommandType} from '../../common/panel_command.js';
import {SettingsManager} from '../../common/settings_manager.js';
import {QueueMode} from '../../common/tts_types.js';
import {ChromeVox} from '../chromevox.js';
import {ChromeVoxPrefs} from '../prefs.js';

interface Offsets {
  brailleOffset: number;
  textOffset: number;
}

type TextToDisplay = [text: string, braille: string];

/**
 * Interface that allows clients to listen for changes to the braille captions.
 */
export interface BrailleCaptionsListener {
  /** Called when the braille captions state changes. */
  onBrailleCaptionsStateChanged(): void;
}

export class BrailleCaptionsBackground {
  static instance: BrailleCaptionsBackground;

  private listener_: BrailleCaptionsListener;

  constructor(listener: BrailleCaptionsListener) {
    this.listener_ = listener;
  }

  /** Called once to initialize the class. */
  static init(listener: BrailleCaptionsListener): void {
    BrailleCaptionsBackground.instance =
        new BrailleCaptionsBackground(listener);
  }

  /** Returns whether the braille captions feature is enabled. */
  static isEnabled(): boolean {
    return Boolean(LocalStorage.get(PREF_KEY));
  }

  /**
   * @param text Text of the shown braille.
   * @param cells Braille cells shown on the display.
   * @param brailleToText Map of Braille letters to the first
   *     index of corresponding text letter.
   * @param offsetsForSlices
   *    Offsets to use for calculating indices. The element is the braille
   * offset, the second is the text offset.
   * @param rows Number of rows to display.
   * @param columns Number of columns to display.
   */
  static setContent(
      text: string, cells: ArrayBuffer, brailleToText: number[],
      offsetsForSlices: Offsets, rows: number, columns: number): void {
    // Convert the cells to Unicode braille pattern characters.
    const byteBuf = new Uint8Array(cells);
    let brailleChars = '';

    for (const byteVal of byteBuf) {
      brailleChars +=
          String.fromCharCode(BRAILLE_UNICODE_BLOCK_START | byteVal);
    }
    const groups = BrailleCaptionsBackground.groupBrailleAndText(
        brailleChars, text, brailleToText, offsetsForSlices);
    const data = {groups, rows, cols: columns};
    (new PanelCommand(PanelCommandType.UPDATE_BRAILLE, data)).send();
  }

  /**
   * @param cells Braille cells shown on the display.
   * @param rows Number of rows to display.
   * @param columns Number of columns to display.
   */
  static setImageContent(
      cells: ArrayBuffer, rows: number, columns: number): void {
    // Convert the cells to Unicode braille pattern characters.
    const byteBuf = new Uint8Array(cells);
    let brailleChars = '';

    for (const byteVal of byteBuf) {
      brailleChars +=
          String.fromCharCode(BRAILLE_UNICODE_BLOCK_START | byteVal);
    }

    const groups = [['Image', brailleChars]];
    const data = {groups, rows, cols: columns};
    (new PanelCommand(PanelCommandType.UPDATE_BRAILLE, data)).send();
  }

  /**
   * @param brailleChars Braille characters shown on the display.
   * @param text Text of the shown braille.
   * @param brailleToText Map of Braille cells to the first index of
   *     corresponding text letter.
   * @param offsets Offsets to use for calculating indices. The element is the
   *     braille offset, the second is the text offset.
   * @return The groups of braille and texts to be displayed.
   */
  static groupBrailleAndText(
      brailleChars: string, text: string, brailleToText: number[],
      offsets: Offsets): TextToDisplay[] {
    let brailleBuf = '';
    const groups: TextToDisplay[] = [];
    let textIndex = 0;
    const brailleOffset = offsets.brailleOffset;
    const textOffset = offsets.textOffset;
    const calculateTextIndex = (index: number): number =>
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
   * @param newValue The new value of the active flag.
   */
  static setActive(newValue: boolean): void {
    const oldValue = BrailleCaptionsBackground.isEnabled();
    // TODO(b/314203187): Not null asserted, check that this is correct.
    ChromeVoxPrefs.instance!.setPref(PREF_KEY, newValue);
    if (oldValue !== newValue) {
      BrailleCaptionsBackground.instance.listener_
          .onBrailleCaptionsStateChanged();
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
   */
  static getVirtualDisplayState(): BrailleDisplayState {
    if (BrailleCaptionsBackground.isEnabled()) {
      const rows = SettingsManager.getNumber('virtualBrailleRows');
      const columns = SettingsManager.getNumber('virtualBrailleColumns');
      // TODO(accessibility) make `cellSize` customizable.
      return {
        available: true,
        textRowCount: rows,
        textColumnCount: columns,
        cellSize: 8,
      };
    } else {
      return {
        available: false,
        textRowCount: 0,
        textColumnCount: 0,
        cellSize: 0,
      };
    }
  }
}

// Local to module.

/** Key set in local storage when this feature is enabled. */
const PREF_KEY = 'brailleCaptions';

/**
 * Unicode block of braille pattern characters.  A braille pattern is formed
 * from this value with the low order 8 bits set to the bits representing
 * the dots as per the ISO 11548-1 standard.
 */
const BRAILLE_UNICODE_BLOCK_START = 0x2800;

TestImportManager.exportForTesting(BrailleCaptionsBackground);
