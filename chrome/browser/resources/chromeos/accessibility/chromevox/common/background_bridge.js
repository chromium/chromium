// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for non-background renderer contexts
 * (options, panel, etc.) to communicate with the background.
 */

goog.provide('BackgroundBridge');

goog.require('BridgeHelper');

BackgroundBridge.BrailleBackground = {
  /**
   * Translate braille cells into text.
   * @param {!ArrayBuffer} cells Cells to be translated.
   * @return {!Promise<?string>}
   */
  async backTranslate(cells) {
    return BridgeHelper.sendMessage(
        'BrailleBackground', 'backTranslate', cells);
  },

  /** @param {string} brailleTable The table for this translator to use. */
  async refreshBrailleTable(brailleTable) {
    return BridgeHelper.sendMessage(
        'BrailleBackground', 'refreshBrailleTable', brailleTable);
  },
};

BackgroundBridge.ChromeVoxPrefs = {
  /**
   * Get the prefs (not including keys).
   * @return {Promise<Object>} A map of all prefs except the key map from
   *     localStorage.
   */
  async getPrefs() {
    return BridgeHelper.sendMessage('ChromeVoxPrefs', 'getPrefs');
  },

  /**
   * Set the value of a pref of logging options.
   * @param {string} key The pref key.
   * @param {boolean} value The new value of the pref.
   */
  async setLoggingPrefs(key, value) {
    return BridgeHelper.sendMessage(
        'ChromeVoxPrefs', 'setLoggingPrefs', {key, value});
  },

  /**
   * Set the value of a pref.
   * @param {string} key The pref key.
   * @param {Object|string|boolean} value The new value of the pref.
   */
  async setPref(key, value) {
    return BridgeHelper.sendMessage('ChromeVoxPrefs', 'setPref', {key, value});
  },
};

BackgroundBridge.PanelBackground = {
  async createNewISearch() {
    return BridgeHelper.sendMessage('PanelBackground', 'createNewISearch');
  },

  /**
   * Performs a search.
   * @param {string} searchStr
   * @param {constants.Dir} dir
   * @param {boolean=} opt_nextObject
   */
  async incrementalSearch(searchStr, dir, opt_nextObject) {
    return BridgeHelper.sendMessage(
        'PanelBackground', 'incrementalSearch',
        {searchStr, dir, opt_nextObject});
  },

  async setRangeToISearchNode() {
    return BridgeHelper.sendMessage('PanelBackground', 'setRangeToISearchNode');
  },
};
