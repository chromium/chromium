// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Braille command definitions.
 * These types are adapted from Chrome's private braille API.
 * They can be found in the Chrome source repo at:
 * src/chrome/common/extensions/api/braille_display_private.idl
 * We define them here since they don't actually exist as bindings under
 * chrome.brailleDisplayPrivate.*.
 */
import {KeyCode} from '/common/key_code.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

interface Modifiers {
  altKey?: boolean;
  ctrlKey?: boolean;
  shiftKey?: boolean;
}

// TODO(b/326623426): Move DomKeyCode to common/.
enum DomKeyCode {
  ARROW_DOWN = 'ArrowDown',
  ARROW_LEFT = 'ArrowLeft',
  ARROW_RIGHT = 'ArrowRight',
  ARROW_UP = 'ArrowUp',
  AUDIO_VOLUME_DOWN = 'AudioVolumeDown',
  AUDIO_VOLUME_UP = 'AudioVolumeUp',
  BACKSPACE = 'Backspace',
  DELETE = 'Delete',
  END = 'End',
  ENTER = 'Enter',
  ESCAPE = 'Escape',
  HOME = 'Home',
  INSERT = 'Insert',
  PAGE_DOWN = 'PageDown',
  PAGE_UP = 'PageUp',
  TAB = 'Tab',
}

/** The set of commands sent from a braille display. */
export import BrailleKeyCommand = chrome.brailleDisplayPrivate.KeyCommand;

/**
 * Represents a key event from a braille display.
 *
 *  command The name of the command.
 *  displayPosition The 0-based position relative to the start of the currently
 *                  displayed text.  Used for commands that involve routing
 *                  keys or similar.  The position is given in characters,
 *                  not braille cells.
 *  brailleDots Dots that were pressed for braille input commands.  Bit mask
 *              where bit 0 represents dot 1 etc.
 * standardKeyCode DOM level 4 key code.
 * standardKeyChar DOM key event character.
 * altKey Whether the alt key was pressed.
 * ctrlKey Whether the control key was pressed.
 * shiftKey Whether the shift key was pressed.
 */
export type BrailleKeyEvent = chrome.brailleDisplayPrivate.KeyEvent;

export namespace BrailleKeyEvent {
  /**
   * Returns the numeric key code for a DOM level 4 key code string.
   * NOTE: Only the key codes produced by the brailleDisplayPrivate API are
   * supported.
   * @param code DOM level 4 key code.
   * @return The numeric key code, or {@code undefined} if unknown.
   */
  export function keyCodeToLegacyCode(code: string): number {
    return BrailleKeyEvent.legacyKeyCodeMap[code];
  }

  /**
   * Returns a char value appropriate for a synthezised key event for a given
   * key code.
   * @param keyCode The DOM level 4 key code.
   * @return Integer character code.
   */
  export function keyCodeToCharValue(keyCode: string): number {
    const SPECIAL_CODES: Record<string, number> = {
      [DomKeyCode.BACKSPACE]: 0x08,
      [DomKeyCode.TAB]: 0x09,
      [DomKeyCode.ENTER]: 0x0A,
    };
    // Note, the Chrome virtual keyboard falls back on the first character of
    // the key code if the key is not one of the above.  Do the same here.
    return SPECIAL_CODES[keyCode] || keyCode.charCodeAt(0);
  }

/*
 * Note: Some of the below mappings contain raw braille dot
 * patterns. These are written out in binary form to make clear
 * exactly what dots in the braille cell make up the pattern. The
 * braille cell is arranged in a 2 by 4 dot grid with each dot
 * assigned a number from 1-8.
 * 1 4
 * 2 5
 * 3 6
 * 7 8
 *
 * In binary form, the dot number minus 1 maps to the bit position
 * (from right to left).
 * For example, dots 1-6-7 would be
 * 0b1100001
 */

  /** Maps a braille pattern to a standard key code. */
  export const brailleDotsToStandardKeyCode: Record<number, string> = {
    0b1: 'A',
    0b11: 'B',
    0b1001: 'C',
    0b11001: 'D',
    0b10001: 'E',
    0b1011: 'F',
    0b11011: 'G',
    0b10011: 'H',
    0b1010: 'I',
    0b11010: 'J',
    0b101: 'K',
    0b111: 'L',
    0b1101: 'M',
    0b11101: 'N',
    0b10101: 'O',
    0b1111: 'P',
    0b11111: 'Q',
    0b10111: 'R',
    0b1110: 'S',
    0b11110: 'T',
    0b100101: 'U',
    0b100111: 'V',
    0b111010: 'W',
    0b101101: 'X',
    0b111101: 'Y',
    0b110101: 'Z',
    0b110100: '0',
    0b10: '1',
    0b110: '2',
    0b10010: '3',
    0b110010: '4',
    0b100010: '5',
    0b10110: '6',
    0b110110: '7',
    0b100110: '8',
    0b10100: '9',
  };

  /** Maps a braille chord pattern to a standard key code. */
  export const brailleChordsToStandardKeyCode: Record<number, string> = {
    0b1000000: DomKeyCode.BACKSPACE,
    0b10100: DomKeyCode.TAB,
    0b110101: DomKeyCode.ESCAPE,
    0b101000: DomKeyCode.ENTER,
  };

  /** Maps a braille dot chord pattern to standard key modifiers. */
  export const brailleDotsToModifiers: Record<number, Modifiers> = {
    0b010010: {ctrlKey: true},
    0b100100: {altKey: true},
    0b1000100: {shiftKey: true},
    0b1010010: {ctrlKey: true, shiftKey: true},
    0b1100100: {altKey: true, shiftKey: true},
  };

  /** Map from DOM level 4 key codes to legacy numeric key codes. */
  export const legacyKeyCodeMap: Record<string, number> = {
    [DomKeyCode.BACKSPACE]: KeyCode.BACK,
    [DomKeyCode.TAB]: KeyCode.TAB,
    [DomKeyCode.ENTER]: KeyCode.RETURN,
    [DomKeyCode.ESCAPE]: KeyCode.ESCAPE,
    [DomKeyCode.HOME]: KeyCode.HOME,
    [DomKeyCode.ARROW_LEFT]: KeyCode.LEFT,
    [DomKeyCode.ARROW_UP]: KeyCode.UP,
    [DomKeyCode.ARROW_RIGHT]: KeyCode.RIGHT,
    [DomKeyCode.ARROW_DOWN]: KeyCode.DOWN,
    [DomKeyCode.PAGE_UP]: KeyCode.PRIOR,
    [DomKeyCode.PAGE_DOWN]: KeyCode.NEXT,
    [DomKeyCode.END]: KeyCode.END,
    [DomKeyCode.INSERT]: KeyCode.INSERT,
    [DomKeyCode.DELETE]: KeyCode.DELETE,
    [DomKeyCode.AUDIO_VOLUME_DOWN]: KeyCode.VOLUME_DOWN,
    [DomKeyCode.AUDIO_VOLUME_UP]: KeyCode.VOLUME_UP,
  };
}

// Add 0-9.
for (let i = '0'.charCodeAt(0); i < '9'.charCodeAt(0); ++i) {
  BrailleKeyEvent.legacyKeyCodeMap[String.fromCharCode(i)] =
      /** @type {Key.Code} */ (i);
}

// Add A-Z.
for (let i = 'A'.charCodeAt(0); i < 'Z'.charCodeAt(0); ++i) {
  BrailleKeyEvent.legacyKeyCodeMap[String.fromCharCode(i)] =
      /** @type {Key.Code} */ (i);
}

// Add the F1 to F12 keys.
for (let i = 0; i < 12; ++i) {
  BrailleKeyEvent.legacyKeyCodeMap['F' + (i + 1)] =
      /** @type {Key.Code} */ (112 + i);
}

/**
 * The state of a braille display as represented in the
 * chrome.brailleDisplayPrivate API.
 * TODO: Convert this to an interface once the typescript migration is complete.
 */
export abstract class BrailleDisplayState {
  abstract available: boolean;
  abstract cellSize: number;
  abstract textColumnCount: number;
  abstract textRowCount: number;
}

TestImportManager.exportForTesting(
    ['BrailleKeyCommand', BrailleKeyCommand],
    ['BrailleKeyEvent', BrailleKeyEvent]);
