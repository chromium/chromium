// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for non-background contexts (options,
 * panel, etc.) to communicate with the background.
 */

goog.provide('BackgroundBridge');

goog.require('BridgeAction');
goog.require('BridgeHelper');
goog.require('BridgeTarget');

BackgroundBridge.BrailleBackground = {
  /**
   * Translate braille cells into text.
   * @param {!ArrayBuffer} cells Cells to be translated.
   * @return {!Promise<?string>}
   */
  async backTranslate(cells) {
    return BridgeHelper.sendMessage(
        BridgeTarget.BRAILLE_BACKGROUND, BridgeAction.BACK_TRANSLATE, cells);
  },

  /** @param {string} brailleTable The table for this translator to use. */
  async refreshBrailleTable(brailleTable) {
    return BridgeHelper.sendMessage(
        BridgeTarget.BRAILLE_BACKGROUND, BridgeAction.REFRESH_BRAILLE_TABLE,
        brailleTable);
  },
};

BackgroundBridge.ChromeVoxBackground = {
  /**
   * Gets the voice currently used by ChromeVox when calling tts.
   * @return {!Promise<string>}
   */
  async getCurrentVoice() {
    return BridgeHelper.sendMessage('ChromeVoxBackground', 'getCurrentVoice');
  },
};

BackgroundBridge.ChromeVoxPrefs = {
  /**
   * Get the prefs (not including keys).
   * @return {Promise<Object>} A map of all prefs except the key map from
   *     localStorage.
   */
  async getPrefs() {
    return BridgeHelper.sendMessage(
        BridgeTarget.CHROMEVOX_PREFS, BridgeAction.GET_PREFS);
  },

  /**
   * Set the value of a pref of logging options.
   * @param {string} key The pref key.
   * @param {boolean} value The new value of the pref.
   */
  async setLoggingPrefs(key, value) {
    return BridgeHelper.sendMessage(
        BridgeTarget.CHROMEVOX_PREFS, BridgeAction.SET_LOGGING_PREFS,
        {key, value});
  },

  /**
   * Set the value of a pref.
   * @param {string} key The pref key.
   * @param {Object|string|boolean} value The new value of the pref.
   */
  async setPref(key, value) {
    return BridgeHelper.sendMessage(
        BridgeTarget.CHROMEVOX_PREFS, BridgeAction.SET_PREF, {key, value});
  },
};

BackgroundBridge.ChromeVoxState = {
  /**
   * Method that updates the punctuation echo level, and also persists setting
   * to local storage.
   * @param {number} punctuationEcho The index of the desired punctuation echo
   * level in AbstractTts.PUNCTUATION_ECHOES.
   */
  async updatePunctuationEcho(punctuationEcho) {
    return BridgeHelper.sendMessage(
        BridgeTarget.CHROMEVOX_STATE, BridgeAction.UPDATE_PUNCTUATION_ECHO,
        punctuationEcho);
  },
};

BackgroundBridge.CommandHandler = {
  /**
   * Handles ChromeVox commands.
   * @param {string} command
   * @return {!Promise<boolean>} True if the command should propagate.
   */
  async onCommand(command) {
    return BridgeHelper.sendMessage(
        BridgeTarget.COMMAND_HANDLER, BridgeAction.ON_COMMAND, command);
  },
};

BackgroundBridge.EventSourceState = {
  /**
   * Gets the current event source.
   * @return {!Promise<EventSourceType>}
   */
  async get() {
    return BridgeHelper.sendMessage(
        BridgeTarget.EVENT_SOURCE_STATE, BridgeAction.GET);
  }
};

BackgroundBridge.LogStore = {
  /** Clear the log buffer. */
  async clearLog() {
    return BridgeHelper.sendMessage(
        BridgeTarget.LOG_STORE, BridgeAction.CLEAR_LOG);
  },

  /**
   * Create logs in order.
   * This function is not currently optimized for speed.
   * @return {!Promise<!Array<BaseLog>>}
   */
  async getLogs() {
    return BridgeHelper.sendMessage(
        BridgeTarget.LOG_STORE, BridgeAction.GET_LOGS);
  },
};
