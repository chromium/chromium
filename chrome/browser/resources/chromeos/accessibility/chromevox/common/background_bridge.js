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

  /**
   * @param {string} brailleTable The table for this translator to use.
   * @return {!Promise<boolean>}
   */
  async refreshBrailleTable(brailleTable) {
    return BridgeHelper.sendMessage(
        BridgeTarget.BRAILLE_BACKGROUND, BridgeAction.REFRESH_BRAILLE_TABLE,
        brailleTable);
  },
};

BackgroundBridge.BrailleCommandHandler = {
  /**
   * @param {boolean} enabled
   * @return {!Promise}
   */
  async setEnabled(enabled) {
    return BridgeHelper.sendMessage(
        Bridgetarget.BRAILLE_COMMAND_HANDLER, BridgeAction.SET_ENABLED,
        enabled);
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
   * @return {!Promise<Object<string, string>>} A map of all prefs except the
   *     key map from localStorage.
   */
  async getPrefs() {
    return BridgeHelper.sendMessage(
        BridgeTarget.CHROMEVOX_PREFS, BridgeAction.GET_PREFS);
  },

  /**
   * Set the value of a pref of logging options.
   * @param {string} key The pref key.
   * @param {boolean} value The new value of the pref.
   * @return {!Promise}
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
   * @return {!Promise}
   */
  async setPref(key, value) {
    return BridgeHelper.sendMessage(
        BridgeTarget.CHROMEVOX_PREFS, BridgeAction.SET_PREF, {key, value});
  },
};

BackgroundBridge.ChromeVoxState = {
  /** @return {!Promise} */
  async clearCurrentRange() {
    return BridgeHelper.sendMessage(
        BridgeTarget.CHROMEVOX_STATE, BridgeAction.CLEAR_CURRENT_RANGE);
  },

  /**
   * Method that updates the punctuation echo level, and also persists setting
   * to local storage.
   * @param {number} punctuationEcho The index of the desired punctuation echo
   *     level in AbstractTts.PUNCTUATION_ECHOES.
   * @return {!Promise}
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
  },
};

BackgroundBridge.GestureCommandHandler = {
  /**
   * @param {boolean} enabled
   * @return {!Promise}
   */
  async setEnabled(enabled) {
    return BridgeHelper.sendMessage(
        BridgeTarget.GESTURE_COMMAND_HANDLER, BridgeAction.SET_ENABLED);
  },
};

BackgroundBridge.EventStreamLogger = {
  /**
   * @param {chrome.automation.EventType} eventType
   * @param {boolean} checked
   * @return {!Promise}
   */
  async notifyEventStreamFilterChanged(name, enabled) {
    return BridgeHelper.sendMessage(
        BridgeTarget.EVENT_STREAM_LOGGER,
        BridgeAction.NOTIFY_EVENT_STREAM_FILTER_CHANGED, {name, enabled});
  },
};

BackgroundBridge.LogStore = {
  /**
   * Clear the log buffer.
   * @return {!Promise}
   */
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

BackgroundBridge.PanelBackground = {
  /** @param {string=} opt_activatedMenuTitle */
  async createAllNodeMenuBackgrounds(opt_activatedMenuTitle) {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND,
        BridgeAction.CREATE_ALL_NODE_MENU_BACKGROUNDS, opt_activatedMenuTitle);
  },

  /**
   * Creates a new ISearch object, ready to search starting from the current
   * ChromeVox focus.
   * @return {!Promise}
   */
  async createNewISearch() {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.CREATE_NEW_I_SEARCH);
  },

  /**
   * Destroy the ISearch object so it can be garbage collected.
   * @return {!Promise}
   */
  async destroyISearch() {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.DESTROY_I_SEARCH);
  },

  /**
   * @param {number} windowId
   * @param {number} tabId
   * @return {!Promise}
   */
  async focusTab(windowId, tabId) {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.FOCUS_TAB,
        {windowId, tabId});
  },

  /**
   * @return {!Promise<{
   *     standardActions: !Array<!chrome.automation.ActionType>,
   *     customActions: !Array<!chrome.automation.CustomAction>
   * }>}
   */
  async getActionsForCurrentNode() {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND,
        BridgeAction.GET_ACTIONS_FOR_CURRENT_NODE);
  },

  /** @return {!Promise<!Array<!PanelTabMenuItemData>>} */
  async getTabMenuData() {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.GET_TAB_MENU_DATA);
  },

  /**
   * @param {string} searchStr
   * @param {constants.Dir} dir
   * @param {boolean=} opt_nextObject
   * @return {!Promise}
   */
  async incrementalSearch(searchStr, dir, opt_nextObject) {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.INCREMENTAL_SEARCH,
        {searchStr, dir, opt_nextObject});
  },

  /**
   * @param {number} callbackNodeIndex
   * @return {!Promise}
   */
  async nodeMenuCallback(callbackNodeIndex) {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.NODE_MENU_CALLBACK,
        callbackNodeIndex);
  },

  /**
   * @param {number} actionId
   * @return {!Promise}
   */
  async performCustomActionOnCurrentNode(actionId) {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND,
        BridgeAction.PERFORM_CUSTOM_ACTION_ON_CURRENT_NODE, actionId);
  },

  /**
   * @param {!chrome.automation.ActionType} action
   * @return {!Promise}
   */
  async performStandardActionOnCurrentNode(action) {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND,
        BridgeAction.PERFORM_STANDARD_ACTION_ON_CURRENT_NODE, action);
  },

  /**
   * Sets the current ChromeVox focus to the current ISearch node.
   * @return {!Promise}
   */
  async setRangeToISearchNode() {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.SET_RANGE_TO_I_SEARCH_NODE);
  },

  /**
   * Listens for focus events, and returns once the target is not the panel.
   */
  async waitForPanelCollapse() {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.WAIT_FOR_PANEL_COLLAPSE);
  },
};

BackgroundBridge.UserActionMonitor = {
  /**
   * Creates a new user action monitor.
   * Resolves after all actions in |actions| have been observed.
   * @param {!Array<{
   *     type: string,
   *     value: (string|Object),
   *     beforeActionMsg: (string|undefined),
   *     afterActionMsg: (string|undefined)}>} actions
   * @return {!Promise}
   */
  async create(actions) {
    return BridgeHelper.sendMessage(
        BridgeTarget.USER_ACTION_MONITOR, BridgeAction.CREATE, actions);
  },

  /**
   * Destroys the user action monitor.
   * @return {!Promise}
   */
  async destroy() {
    return BridgeHelper.sendMessage(
        BridgeTarget.USER_ACTION_MONITOR, BridgeAction.DESTROY);
  },
};
