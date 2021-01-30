// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox keyboard handler.
 */

goog.provide('BackgroundKeyboardHandler');

goog.require('ChromeVoxState');
goog.require('EventSourceState');
goog.require('KeyCode');
goog.require('MathHandler');
goog.require('Output');
goog.require('ChromeVoxKbHandler');
goog.require('ChromeVoxPrefs');

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
  PENDING_SHORTCUT_KEYUPS: 'pending_shortcut_keyups'
};

BackgroundKeyboardHandler = class {
  constructor() {
    /** @private {!KeyboardPassThroughState_} */
    this.passThroughState_ = KeyboardPassThroughState_.NO_PASS_THROUGH;

    /** @type {Set} @private */
    this.eatenKeyDowns_ = new Set();

    /** @private {Set} */
    this.passedThroughKeyDowns_ = new Set();

    document.addEventListener('keydown', this.onKeyDown.bind(this), false);
    document.addEventListener('keyup', this.onKeyUp.bind(this), false);

    chrome.accessibilityPrivate.setKeyboardListener(
        true, ChromeVox.isStickyPrefOn);
  }

  /**
   * Handles key down events.
   * @param {Event} evt The key down event to process.
   * @return {boolean} This value has no effect since we ignore it in
   *     SpokenFeedbackEventRewriterDelegate::HandleKeyboardEvent.
   */
  onKeyDown(evt) {
    EventSourceState.set(EventSourceType.STANDARD_KEYBOARD);
    evt.stickyMode = ChromeVox.isStickyModeOn();

    // If somehow the user gets into a state where there are dangling key downs
    // don't get a key up, clear the eaten key downs. This is detected by a set
    // list of modifier flags.
    if (!evt.altKey && !evt.ctrlKey && !evt.metaKey && !evt.shiftKey) {
      this.eatenKeyDowns_.clear();
      this.passedThroughKeyDowns_.clear();
    }

    if (ChromeVox.passThroughMode) {
      this.passedThroughKeyDowns_.add(evt.keyCode);
      return false;
    }

    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);

    // Defer first to the math handler, if it exists, then ordinary keyboard
    // commands.
    if (!MathHandler.onKeyDown(evt) ||
        !ChromeVoxKbHandler.basicKeyDownActionsListener(evt) ||
        // We natively always capture Search, so we have to be very careful to
        // either eat it here or re-inject it; otherwise, some components, like
        // ARC++ with TalkBack never get it. We only want to re-inject when
        // ChromeVox has no range.
        (ChromeVoxState.instance.currentRange &&
         (evt.metaKey || evt.keyCode === KeyCode.SEARCH))) {
      if (ChromeVox.passThroughMode) {
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

    if (ChromeVox.passThroughMode) {
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
        ChromeVox.passThroughMode = false;
        this.passThroughState_ = KeyboardPassThroughState_.NO_PASS_THROUGH;
      }
    }

    return false;
  }
};
