// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {isChromeOS, isMac} from 'chrome://resources/js/cr.m.js';


/** @enum {number} */
export const Key = {
  Comma: 188,
  Del: 46,
  Down: 40,
  End: 35,
  Escape: 27,
  Home: 36,
  Ins: 45,
  Left: 37,
  MediaNextTrack: 176,
  MediaPlayPause: 179,
  MediaPrevTrack: 177,
  MediaStop: 178,
  PageDown: 34,
  PageUp: 33,
  Period: 190,
  Right: 39,
  Space: 32,
  Tab: 9,
  Up: 38,
};

/**
 * Enum for whether we require modifiers of a keycode.
 * @enum {number}
 */
const ModifierPolicy = {
  NOT_ALLOWED: 0,
  REQUIRED: 1
};

/**
 * Gets the ModifierPolicy. Currently only "MediaNextTrack", "MediaPrevTrack",
 * "MediaStop", "MediaPlayPause" are required to be used without any modifier.
 * @param {number} keyCode
 * @return {ModifierPolicy}
 */
function getModifierPolicy(keyCode) {
  switch (keyCode) {
    case Key.MediaNextTrack:
    case Key.MediaPlayPause:
    case Key.MediaPrevTrack:
    case Key.MediaStop:
      return ModifierPolicy.NOT_ALLOWED;
    default:
      return ModifierPolicy.REQUIRED;
  }
}

/**
 * Returns whether the keyboard event has a key modifier, which could affect
 * how it's handled.
 * @param {!KeyboardEvent} e
 * @param {boolean} countShiftAsModifier Whether the 'Shift' key should be
 *     counted as modifier.
 * @return {boolean} True if the event has any modifiers.
 */
function hasModifier(e, countShiftAsModifier) {
  return e.ctrlKey || e.altKey ||
      // Meta key is only relevant on Mac and CrOS, where we treat Command
      // and Search (respectively) as modifiers.
      (isMac && e.metaKey) || (isChromeOS && e.metaKey) ||
      (countShiftAsModifier && e.shiftKey);
}

/**
 * Checks whether the passed in |keyCode| is a valid extension command key.
 * @param {number} keyCode
 * @return {boolean} Whether the key is valid.
 */
export function isValidKeyCode(keyCode) {
  if (keyCode == Key.Escape) {
    return false;
  }
  for (const k in Key) {
    if (Key[k] == keyCode) {
      return true;
    }
  }
  return (keyCode >= 'A'.charCodeAt(0) && keyCode <= 'Z'.charCodeAt(0)) ||
      (keyCode >= '0'.charCodeAt(0) && keyCode <= '9'.charCodeAt(0));
}

/**
 * Converts a keystroke event to string form, ignoring invalid extension
 * commands.
 * @param {!KeyboardEvent} e
 * @return {string} The keystroke as a string.
 */
export function keystrokeToString(e) {
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
        case Key.Comma:
          output.push('Comma');
          break;
        case Key.Del:
          output.push('Delete');
          break;
        case Key.Down:
          output.push('Down');
          break;
        case Key.End:
          output.push('End');
          break;
        case Key.Home:
          output.push('Home');
          break;
        case Key.Ins:
          output.push('Insert');
          break;
        case Key.Left:
          output.push('Left');
          break;
        case Key.MediaNextTrack:
          output.push('MediaNextTrack');
          break;
        case Key.MediaPlayPause:
          output.push('MediaPlayPause');
          break;
        case Key.MediaPrevTrack:
          output.push('MediaPrevTrack');
          break;
        case Key.MediaStop:
          output.push('MediaStop');
          break;
        case Key.PageDown:
          output.push('PageDown');
          break;
        case Key.PageUp:
          output.push('PageUp');
          break;
        case Key.Period:
          output.push('Period');
          break;
        case Key.Right:
          output.push('Right');
          break;
        case Key.Space:
          output.push('Space');
          break;
        case Key.Tab:
          output.push('Tab');
          break;
        case Key.Up:
          output.push('Up');
          break;
      }
    }
  }

  return output.join('+');
}

/**
 * Returns true if the event has valid modifiers.
 * @param {!KeyboardEvent} e The keyboard event to consider.
 * @return {boolean} True if the event is valid.
 */
export function hasValidModifiers(e) {
  switch (getModifierPolicy(e.keyCode)) {
    case ModifierPolicy.REQUIRED:
      return hasModifier(e, false);
    case ModifierPolicy.NOT_ALLOWED:
      return !hasModifier(e, true);
  }
  assertNotReached();
}
