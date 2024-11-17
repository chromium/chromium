// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {isChromeOS, isMac} from 'chrome://resources/js/platform.js';


export enum Key {
  COMMA = 188,
  DEL = 46,
  DOWN = 40,
  END = 35,
  ESCAPE = 27,
  HOME = 36,
  INS = 45,
  LEFT = 37,
  MEDIA_NEXT_TRACK = 176,
  MEDIA_PLAY_PAUSE = 179,
  MEDIA_PREV_TRACK = 177,
  MEDIA_STOP = 178,
  PAGE_DOWN = 34,
  PAGE_UP = 33,
  PERIOD = 190,
  RIGHT = 39,
  SPACE = 32,
  TAB = 9,
  UP = 38,
}

/**
 * Enum for whether we require modifiers of a keycode.
 */
enum ModifierPolicy {
  NOT_ALLOWED = 0,
  REQUIRED = 1
}

/**
 * Gets the ModifierPolicy. Currently only "MediaNextTrack", "MediaPrevTrack",
 * "MediaStop", "MediaPlayPause" are required to be used without any modifier.
 */
function getModifierPolicy(keyCode: number): ModifierPolicy {
  switch (keyCode) {
    case Key.MEDIA_NEXT_TRACK:
    case Key.MEDIA_PLAY_PAUSE:
    case Key.MEDIA_PREV_TRACK:
    case Key.MEDIA_STOP:
      return ModifierPolicy.NOT_ALLOWED;
    default:
      return ModifierPolicy.REQUIRED;
  }
}

/**
 * Returns whether the keyboard event has a key modifier, which could affect
 * how it's handled.
 * @param countShiftAsModifier Whether the 'Shift' key should be counted as
 *     modifier.
 * @return Whether the event has any modifiers.
 */
function hasModifier(e: KeyboardEvent, countShiftAsModifier: boolean): boolean {
  return e.ctrlKey || e.altKey ||
      // Meta key is only relevant on Mac and CrOS, where we treat Command
      // and Search (respectively) as modifiers.
      (isMac && e.metaKey) || (isChromeOS && e.metaKey) ||
      (countShiftAsModifier && e.shiftKey);
}

/**
 * Checks whether the passed in |keyCode| is a valid extension command key.
 * @return Whether the key is valid.
 */
export function isValidKeyCode(keyCode: number): boolean {
  if (keyCode === Key.ESCAPE) {
    return false;
  }
  for (const k in Key) {
    if (Key[k as keyof typeof Key] === keyCode) {
      return true;
    }
  }
  return (keyCode >= 'A'.charCodeAt(0) && keyCode <= 'Z'.charCodeAt(0)) ||
      (keyCode >= '0'.charCodeAt(0) && keyCode <= '9'.charCodeAt(0));
}

/**
 * Converts a keystroke event to string form, ignoring invalid extension
 * commands.
 */
export function keystrokeToString(e: KeyboardEvent): string {
  const output = [];
  // TODO(devlin): Should this be i18n'd?
  if (isMac && e.metaKey) {
    output.push('Command');
  }
  if (isChromeOS && e.metaKey) {
    output.push('Search');
  }
  if (e.ctrlKey) {
    output.push('Ctrl');
  }
  if (!e.ctrlKey && e.altKey) {
    output.push('Alt');
  }
  if (e.shiftKey) {
    output.push('Shift');
  }

  const keyCode = e.keyCode;
  if (isValidKeyCode(keyCode)) {
    if ((keyCode >= 'A'.charCodeAt(0) && keyCode <= 'Z'.charCodeAt(0)) ||
        (keyCode >= '0'.charCodeAt(0) && keyCode <= '9'.charCodeAt(0))) {
      output.push(String.fromCharCode(keyCode));
    } else {
      switch (keyCode) {
        case Key.COMMA:
          output.push('Comma');
          break;
        case Key.DEL:
          output.push('Delete');
          break;
        case Key.DOWN:
          output.push('Down');
          break;
        case Key.END:
          output.push('End');
          break;
        case Key.HOME:
          output.push('Home');
          break;
        case Key.INS:
          output.push('Insert');
          break;
        case Key.LEFT:
          output.push('Left');
          break;
        case Key.MEDIA_NEXT_TRACK:
          output.push('MediaNextTrack');
          break;
        case Key.MEDIA_PLAY_PAUSE:
          output.push('MediaPlayPause');
          break;
        case Key.MEDIA_PREV_TRACK:
          output.push('MediaPrevTrack');
          break;
        case Key.MEDIA_STOP:
          output.push('MediaStop');
          break;
        case Key.PAGE_DOWN:
          output.push('PageDown');
          break;
        case Key.PAGE_UP:
          output.push('PageUp');
          break;
        case Key.PERIOD:
          output.push('Period');
          break;
        case Key.RIGHT:
          output.push('Right');
          break;
        case Key.SPACE:
          output.push('Space');
          break;
        case Key.TAB:
          output.push('Tab');
          break;
        case Key.UP:
          output.push('Up');
          break;
      }
    }
  }

  return output.join('+');
}

/**
 * Returns true if the event has valid modifiers.
 * @param e The keyboard event to consider.
 * @return Wether the event is valid.
 */
export function hasValidModifiers(e: KeyboardEvent): boolean {
  switch (getModifierPolicy(e.keyCode)) {
    case ModifierPolicy.REQUIRED:
      return hasModifier(e, false);
    case ModifierPolicy.NOT_ALLOWED:
      return !hasModifier(e, true);
    default:
      assertNotReached();
  }
}

export function formatShortcutText(text: string): string {
  return text.split('+').join(' + ');
}
