// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface for querying and modifying the global
 *     ChromeVox state, to avoid direct dependencies on the Background
 *     object and to facilitate mocking for tests.
 */

goog.provide('ChromeVoxState');
goog.provide('ChromeVoxStateObserver');

goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('BrailleKeyEvent');
goog.require('BridgeHelper');
goog.require('UserActionMonitor');

/**
 * An interface implemented by objects that want to observe ChromeVox state
 * changes.
 * @interface
 */
ChromeVoxStateObserver = class {
  /**
   * @param {cursors.Range} range The new range.
   * @param {boolean=} opt_fromEditing
   */
  onCurrentRangeChanged(range, opt_fromEditing) {}
};

ChromeVoxState = class {
  /** @return {cursors.Range} */
  get currentRange() {
    return this.getCurrentRange();
  }

  /**
   * @return {cursors.Range} The current range.
   * @protected
   */
  getCurrentRange() {
    return null;
  }

  /** @return {cursors.Range} */
  get pageSel() {
    return null;
  }

  /**
   * Return the current range, but focus recovery is not applied to it.
   * @return {cursors.Range} The current range.
   * @abstract
   */
  getCurrentRangeWithoutRecovery() {}

  /**
   * @param {cursors.Range} newRange The new range.
   * @param {boolean=} opt_fromEditing
   * @abstract
   */
  setCurrentRange(newRange, opt_fromEditing) {}

  /**
   * @param {cursors.Range}
   * @abstract
   */
  set pageSel(newPageSel) {}

  /** @return {number} */
  get typingEcho() {}

  /** @param {number} newTypingEcho */
  set typingEcho(newTypingEcho) {}

  /**
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!cursors.Range} range The new range.
   * @param {boolean=} opt_focus Focus the range; defaults to true.
   * @param {Object=} opt_speechProps Speech properties.
   * @param {boolean=} opt_skipSettingSelection If true, does not set
   *     the selection, otherwise it does by default.
   * @abstract
   */
  navigateToRange(range, opt_focus, opt_speechProps, opt_skipSettingSelection) {
  }

  /**
   * Restores the last valid ChromeVox range.
   * @abstract
   */
  restoreLastValidRangeIfNeeded() {}

  /**
   * Handles a braille command.
   * @param {!BrailleKeyEvent} evt
   * @param {!NavBraille} content
   * @return {boolean} True if evt was processed.
   * @abstract
   */
  onBrailleKeyEvent(evt, content) {}

  /**
   * Forces the reading of the next change to the clipboard.
   * @abstract
   */
  readNextClipboardDataChange() {}
};

/** @type {ChromeVoxState} */
ChromeVoxState.instance;

/**
 * Holds the un-composite tts object.
 * @type {Object}
 */
ChromeVoxState.backgroundTts;

/** @type {boolean} */
ChromeVoxState.isReadingContinuously;

/** @type {!Array<ChromeVoxStateObserver>} */
ChromeVoxState.observers = [];

/** @param {ChromeVoxStateObserver} observer */
ChromeVoxState.addObserver = function(observer) {
  ChromeVoxState.observers.push(observer);
};

/** @param {ChromeVoxStateObserver} observer */
ChromeVoxState.removeObserver = function(observer) {
  const index = ChromeVoxState.observers.indexOf(observer);
  if (index > -1) {
    ChromeVoxState.observers.splice(index, 1);
  }
};

BridgeHelper.registerHandler(
    BridgeTargets.CHROMEVOX_STATE, BridgeActions.CLEAR_CURRENT_RANGE,
    () => ChromeVoxState.instance.setCurrentRange(null));
BridgeHelper.registerHandler(
    BridgeTargets.CHROMEVOX_STATE, BridgeActions.UPDATE_PUNCTUATION_ECHO,
    (echo) => ChromeVoxState.backgroundTts.updatePunctuationEcho(echo));
