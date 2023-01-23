// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox keyboard handler.
 */
import {KeyCode} from '../../common/key_code.js';
import {EventSourceType} from '../common/event_source_type.js';
import {ChromeVoxKbHandler} from '../common/keyboard_handler.js';
import {Msgs} from '../common/msgs.js';
import {QueueMode} from '../common/tts_types.js';

import {ChromeVox} from './chromevox.js';
import {ChromeVoxRange} from './chromevox_range.js';
import {ChromeVoxState} from './chromevox_state.js';
import {EventSource} from './event_source.js';
import {MathHandler} from './math_handler.js';
import {Output} from './output/output.js';
import {ChromeVoxPrefs} from './prefs.js';
import {UserActionMonitor} from './user_action_monitor.js';

/**
 * @enum {string}
 * Internal pass through mode state (see usage below).
 * @private
 */
const KeyboardPassThroughState_ = {
  // No pass through is in progress.
  NO_PASS_THROUGH: 'no_pass_through',

  // The pass through shortcut command has been pressed (keydowns), waiting for
  // user to release (keyups) all the shortcut keys.
  PENDING_PASS_THROUGH_SHORTCUT_KEYUPS: 'pending_pass_through_keyups',

  // The pass through shortcut command has been pressed and released, waiting
  // for the user to press/release a shortcut to be passed through.
  PENDING_SHORTCUT_KEYUPS: 'pending_shortcut_keyups',
};

export class BackgroundKeyboardHandler {
  /** @private */
  constructor() {
    /** @private {Set} */
    this.eatenKeyDowns_ = new Set();

    /** @private {boolean} */
    this.passThroughModeEnabled_ = false;

    /** @private {!KeyboardPassThroughState_} */
    this.passThroughState_ = KeyboardPassThroughState_.NO_PASS_THROUGH;

    /** @private {Set} */
    this.passedThroughKeyDowns_ = new Set();

    document.addEventListener(
        'keydown', (event) => this.onKeyDown(event), false);
    document.addEventListener('keyup', (event) => this.onKeyUp(event), false);

    chrome.accessibilityPrivate.setKeyboardListener(
        true, ChromeVoxPrefs.isStickyPrefOn);
  }

  static init() {
    if (BackgroundKeyboardHandler.instance) {
      throw 'Error: trying to create two instances of singleton BackgroundKeyboardHandler.';
    }
    BackgroundKeyboardHandler.instance = new BackgroundKeyboardHandler();
  }

  static enablePassThroughMode() {
    ChromeVox.tts.speak(Msgs.getMsg('pass_through_key'), QueueMode.QUEUE);
    BackgroundKeyboardHandler.instance.passThroughModeEnabled_ = true;
  }

  /**
   * Handles key down events.
   * @param {Event} evt The key down event to process.
   * @return {boolean} This value has no effect since we ignore it in
   *     SpokenFeedbackEventRewriterDelegate::HandleKeyboardEvent.
   */
  onKeyDown(evt) {
    EventSource.set(EventSourceType.STANDARD_KEYBOARD);
    evt.stickyMode = ChromeVoxPrefs.isStickyModeOn();

    // If somehow the user gets into a state where there are dangling key downs
    // don't get a key up, clear the eaten key downs. This is detected by a set
    // list of modifier flags.
    if (!evt.altKey && !evt.ctrlKey && !evt.metaKey && !evt.shiftKey) {
      this.eatenKeyDowns_.clear();
      this.passedThroughKeyDowns_.clear();
    }

    if (this.passThroughModeEnabled_) {
      this.passedThroughKeyDowns_.add(evt.keyCode);
      return false;
    }

    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);

    // Try to restore to the last valid range.
    ChromeVoxState.instance.restoreLastValidRangeIfNeeded();

    if (!this.callOnKeyDownHandlers_(evt) ||
        this.shouldConsumeSearchKey_(evt)) {
      if (this.passThroughModeEnabled_) {
        this.passThroughState_ =
            KeyboardPassThroughState_.PENDING_PASS_THROUGH_SHORTCUT_KEYUPS;
      }
      evt.preventDefault();
      evt.stopPropagation();
      this.eatenKeyDowns_.add(evt.keyCode);
    }
    return false;
  }

  /**
   * @param {Event} evt The key down event to process.
   * @return {boolean} Whether the event should continue propagating.
   * @private
   */
  callOnKeyDownHandlers_(evt) {
    // Defer first to the math handler, if it exists, then ordinary keyboard
    // commands.
    if (!MathHandler.onKeyDown(evt)) {
      return false;
    }

    const userActionMonitor = UserActionMonitor.instance;
    if (userActionMonitor && !userActionMonitor.onKeyDown(evt)) {
      return false;
    }

    return ChromeVoxKbHandler.basicKeyDownActionsListener(evt);
  }

  /**
   * @param {Event} evt The key down event to evaluate.
   * @return {boolean} Whether the event should be consumed.
   * @private
   */
  shouldConsumeSearchKey_(evt) {
    // We natively always capture Search, so we have to be very careful to
    // either eat it here or re-inject it; otherwise, some components, like
    // ARC++ with TalkBack never get it. We only want to re-inject when
    // ChromeVox has no range.
    if (!ChromeVoxRange.current) {
      return false;
    }

    return Boolean(evt.metaKey) || evt.keyCode === KeyCode.SEARCH;
  }

  /**
   * Handles key up events.
   * @param {Event} evt The key up event to process.
   * @return {boolean} This value has no effect since we ignore it in
   *     SpokenFeedbackEventRewriterDelegate::HandleKeyboardEvent.
   */
  onKeyUp(evt) {
    if (this.eatenKeyDowns_.has(evt.keyCode)) {
      evt.preventDefault();
      evt.stopPropagation();
      this.eatenKeyDowns_.delete(evt.keyCode);
    }

    if (this.passThroughModeEnabled_) {
      this.passedThroughKeyDowns_.delete(evt.keyCode);
      if (this.passThroughState_ ===
              KeyboardPassThroughState_.PENDING_PASS_THROUGH_SHORTCUT_KEYUPS &&
          this.eatenKeyDowns_.size === 0) {
        // All keys of the pass through shortcut command have been released.
        // Ready to pass through the next shortcut.
        this.passThroughState_ =
            KeyboardPassThroughState_.PENDING_SHORTCUT_KEYUPS;
      } else if (
          this.passThroughState_ ===
              KeyboardPassThroughState_.PENDING_SHORTCUT_KEYUPS &&
          this.passedThroughKeyDowns_.size === 0) {
        // All keys of the passed through shortcut have been released. Ready to
        // go back to normal processing (aka no pass through).
        this.passThroughModeEnabled_ = false;
        this.passThroughState_ = KeyboardPassThroughState_.NO_PASS_THROUGH;
      }
    }

    return false;
  }
}

/** @type {BackgroundKeyboardHandler} */
BackgroundKeyboardHandler.instance;
