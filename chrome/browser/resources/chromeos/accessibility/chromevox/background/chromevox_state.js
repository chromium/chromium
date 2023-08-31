// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface for querying and modifying the global
 *     ChromeVox state, to avoid direct dependencies on the Background
 *     object and to facilitate mocking for tests.
 */
import {constants} from '../../common/constants.js';
import {CursorRange} from '../../common/cursors/range.js';
import {BrailleKeyEvent} from '../common/braille/braille_key_types.js';
import {NavBraille} from '../common/braille/nav_braille.js';
import {TtsSpeechProperties} from '../common/tts_types.js';

export class ChromeVoxState {
  /** @return {!Promise} */
  static ready() {
    return ChromeVoxState.readyPromise_;
  }

  /** Can be overridden to initialize values and state when first created. */
  init() {}

  /** @return {boolean} */
  get isReadingContinuously() {
    return false;
  }

  /** @return {CursorRange} */
  get pageSel() {
    return null;
  }

  /** @return {boolean} */
  get talkBackEnabled() {
    return false;
  }

  /**
   * @param {boolean} newValue
   */
  set isReadingContinuously(newValue) {}

  /**
   * @param {CursorRange} newPageSel
   */
  set pageSel(newPageSel) {}

  /**
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!CursorRange} range The new range.
   * @param {boolean=} opt_focus Focus the range; defaults to true.
   * @param {TtsSpeechProperties=} opt_speechProps Speech properties.
   * @param {boolean=} opt_skipSettingSelection If true, does not set
   *     the selection, otherwise it does by default.
   */
  navigateToRange(range, opt_focus, opt_speechProps, opt_skipSettingSelection) {
  }

  /**
   * Restores the last valid ChromeVox range.
   */
  restoreLastValidRangeIfNeeded() {}

  /**
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   */
  setFocusToRange(range, prevRange) {}

  /**
   * Handles a braille command.
   * @param {!BrailleKeyEvent} evt
   * @param {!NavBraille} content
   * @return {boolean} True if evt was processed.
   */
  onBrailleKeyEvent(evt, content) {}
}

/** @type {ChromeVoxState} */
ChromeVoxState.instance;

/** @type {!Object<string, constants.Point>} */
ChromeVoxState.position = {};

/** @protected {function()} */
ChromeVoxState.resolveReadyPromise_;
/** @private {!Promise} */
ChromeVoxState.readyPromise_ =
    new Promise(resolve => ChromeVoxState.resolveReadyPromise_ = resolve);
