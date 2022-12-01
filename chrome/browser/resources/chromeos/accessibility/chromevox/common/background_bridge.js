// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for non-background contexts (options,
 * panel, etc.) to communicate with the background.
 */

import {constants} from '../../common/constants.js';

import {BridgeConstants} from './bridge_constants.js';
import {BridgeHelper} from './bridge_helper.js';
import {Command} from './command_store.js';
import {BaseLog, SerializableLog} from './log_types.js';
import {PanelTabMenuItemData} from './panel_menu_data.js';
import {QueueMode, TtsSpeechProperties} from './tts_types.js';

export const BackgroundBridge = {};

BackgroundBridge.BrailleBackground = {
  /**
   * Translate braille cells into text.
   * @param {!ArrayBuffer} cells Cells to be translated.
   * @return {!Promise<?string>}
   */
  async backTranslate(cells) {
    return BridgeHelper.sendMessage(
        BridgeConstants.BrailleBackground.TARGET,
        BridgeConstants.BrailleBackground.Action.BACK_TRANSLATE, cells);
  },

  /**
   * @param {string} brailleTable The table for this translator to use.
   * @return {!Promise<boolean>}
   */
  async refreshBrailleTable(brailleTable) {
    return BridgeHelper.sendMessage(
        BridgeConstants.BrailleBackground.TARGET,
        BridgeConstants.BrailleBackground.Action.REFRESH_BRAILLE_TABLE,
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
        BridgeConstants.BrailleCommandHandler.TARGET,
        BridgeConstants.BrailleCommandHandler.Action.SET_ENABLED, enabled);
  },
};

BackgroundBridge.ChromeVoxPrefs = {
  /**
   * Get the prefs (not including keys).
   * @return {!Promise<Object<string, string>>} A map of all prefs except the
   *     key map from LocalStorage.
   */
  async getPrefs() {
    return BridgeHelper.sendMessage(
        BridgeConstants.ChromeVoxPrefs.TARGET,
        BridgeConstants.ChromeVoxPrefs.Action.GET_PREFS);
  },

  /** @return {!Promise<boolean>} */
  async getStickyPref() {
    return BridgeHelper.sendMessage(
        BridgeConstants.ChromeVoxPrefs.TARGET,
        BridgeConstants.ChromeVoxPrefs.Action.GET_STICKY_PREF);
  },

  /**
   * Set the value of a pref of logging options.
   * @param {string} key The pref key.
   * @param {boolean} value The new value of the pref.
   * @return {!Promise}
   */
  async setLoggingPrefs(key, value) {
    return BridgeHelper.sendMessage(
        BridgeConstants.ChromeVoxPrefs.TARGET,
        BridgeConstants.ChromeVoxPrefs.Action.SET_LOGGING_PREFS, key, value);
  },

  /**
   * Set the value of a pref.
   * @param {string} key The pref key.
   * @param {Object|string|boolean} value The new value of the pref.
   * @return {!Promise}
   */
  async setPref(key, value) {
    return BridgeHelper.sendMessage(
        BridgeConstants.ChromeVoxPrefs.TARGET,
        BridgeConstants.ChromeVoxPrefs.Action.SET_PREF, key, value);
  },
};

BackgroundBridge.ChromeVoxState = {
  /** @return {!Promise} */
  async clearCurrentRange() {
    return BridgeHelper.sendMessage(
        BridgeConstants.ChromeVoxState.TARGET,
        BridgeConstants.ChromeVoxState.Action.CLEAR_CURRENT_RANGE);
  },
};

BackgroundBridge.CommandHandler = {
  /**
   * Handles ChromeVox commands.
   * @param {!Command} command
   * @return {!Promise<boolean>} True if the command should propagate.
   */
  async onCommand(command) {
    return BridgeHelper.sendMessage(
        BridgeConstants.CommandHandler.TARGET,
        BridgeConstants.CommandHandler.Action.ON_COMMAND, command);
  },
};

BackgroundBridge.EventSourceState = {
  /**
   * Gets the current event source.
   * TODO(accessibility): this type is ES6; replace once possible.
   * @return {!Promise<string>}
   */
  async get() {
    return BridgeHelper.sendMessage(
        BridgeConstants.EventSourceState.TARGET,
        BridgeConstants.EventSourceState.Action.GET);
  },
};

BackgroundBridge.GestureCommandHandler = {
  /**
   * @param {boolean} enabled
   * @return {!Promise}
   */
  async setEnabled(enabled) {
    return BridgeHelper.sendMessage(
        BridgeConstants.GestureCommandHandler.TARGET,
        BridgeConstants.GestureCommandHandler.Action.SET_ENABLED);
  },
};

BackgroundBridge.EventStreamLogger = {
  /**
   * @param {chrome.automation.EventType} name
   * @param {boolean} enabled
   * @return {!Promise}
   */
  async notifyEventStreamFilterChanged(name, enabled) {
    return BridgeHelper.sendMessage(
        BridgeConstants.EventStreamLogger.TARGET,
        BridgeConstants.EventStreamLogger.Action
            .NOTIFY_EVENT_STREAM_FILTER_CHANGED,
        name, enabled);
  },
};

BackgroundBridge.LogStore = {
  /**
   * Clear the log buffer.
   * @return {!Promise}
   */
  async clearLog() {
    return BridgeHelper.sendMessage(
        BridgeConstants.LogStore.TARGET,
        BridgeConstants.LogStore.Action.CLEAR_LOG);
  },

  /**
   * Create logs in order.
   * This function is not currently optimized for speed.
   * @return {!Promise<!Array<!SerializableLog>>}
   */
  async getLogs() {
    return BridgeHelper.sendMessage(
        BridgeConstants.LogStore.TARGET,
        BridgeConstants.LogStore.Action.GET_LOGS);
  },
};

BackgroundBridge.PanelBackground = {
  /** @return {!Promise} */
  async clearSavedNode() {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.CLEAR_SAVED_NODE);
  },

  /** @param {*=} opt_activatedMenuTitle */
  async createAllNodeMenuBackgrounds(opt_activatedMenuTitle) {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.CREATE_ALL_NODE_MENU_BACKGROUNDS,
        opt_activatedMenuTitle);
  },

  /**
   * Creates a new ISearch object, ready to search starting from the current
   * ChromeVox focus.
   * @return {!Promise}
   */
  async createNewISearch() {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.CREATE_NEW_I_SEARCH);
  },

  /**
   * Destroy the ISearch object so it can be garbage collected.
   * @return {!Promise}
   */
  async destroyISearch() {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.DESTROY_I_SEARCH);
  },

  /**
   * @param {number} windowId
   * @param {number} tabId
   * @return {!Promise}
   */
  async focusTab(windowId, tabId) {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.FOCUS_TAB, windowId, tabId);
  },

  /**
   * @return {!Promise<{
   *     standardActions: !Array<!chrome.automation.ActionType>,
   *     customActions: !Array<!chrome.automation.CustomAction>
   * }>}
   */
  async getActionsForCurrentNode() {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.GET_ACTIONS_FOR_CURRENT_NODE);
  },

  /** @return {!Promise<!Array<!PanelTabMenuItemData>>} */
  async getTabMenuData() {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.GET_TAB_MENU_DATA);
  },

  /**
   * @param {string} searchStr
   * @param {constants.Dir} dir
   * @param {boolean=} opt_nextObject
   * @return {!Promise}
   */
  async incrementalSearch(searchStr, dir, opt_nextObject) {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.INCREMENTAL_SEARCH, searchStr,
        dir, opt_nextObject);
  },

  /**
   * @param {number} actionId
   * @return {!Promise}
   */
  async performCustomActionOnCurrentNode(actionId) {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action
            .PERFORM_CUSTOM_ACTION_ON_CURRENT_NODE,
        actionId);
  },

  /**
   * @param {!chrome.automation.ActionType} action
   * @return {!Promise}
   */
  async performStandardActionOnCurrentNode(action) {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action
            .PERFORM_STANDARD_ACTION_ON_CURRENT_NODE,
        action);
  },

  /** @return {!Promise} */
  async saveCurrentNode() {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.SAVE_CURRENT_NODE);
  },

  /**
   * Adds an event listener to detect panel collapse.
   */
  async setPanelCollapseWatcher() {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.SET_PANEL_COLLAPSE_WATCHER);
  },

  /**
   * Sets the current ChromeVox focus to the current ISearch node.
   * @return {!Promise}
   */
  async setRangeToISearchNode() {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.SET_RANGE_TO_I_SEARCH_NODE);
  },

  /**
   * Wait for the promise to notify panel collapse to resolved
   */
  async waitForPanelCollapse() {
    return BridgeHelper.sendMessage(
        BridgeConstants.PanelBackground.TARGET,
        BridgeConstants.PanelBackground.Action.WAIT_FOR_PANEL_COLLAPSE);
  },
};

BackgroundBridge.TtsBackground = {
  /**
   * Gets the voice currently used by ChromeVox when calling tts.
   * @return {!Promise<string>}
   */
  async getCurrentVoice() {
    return BridgeHelper.sendMessage(
        BridgeConstants.TtsBackground.TARGET,
        BridgeConstants.TtsBackground.Action.GET_CURRENT_VOICE);
  },

  /**
   * @param {string} textString The string of text to be spoken.
   * @param {QueueMode} queueMode The queue mode to use for speaking.
   * @param {TtsSpeechProperties=} properties Speech properties to use for
   *     this utterance.
   */
  async speak(textString, queueMode, properties) {
    return BridgeHelper.sendMessage(
        BridgeConstants.TtsBackground.TARGET,
        BridgeConstants.TtsBackground.Action.SPEAK, textString, queueMode,
        properties);
  },

  /**
   * Method that updates the punctuation echo level, and also persists setting
   * to local storage.
   * @param {number} punctuationEcho The index of the desired punctuation echo
   *     level in PunctuationEchoes.
   * @return {!Promise}
   */
  async updatePunctuationEcho(punctuationEcho) {
    return BridgeHelper.sendMessage(
        BridgeConstants.TtsBackground.TARGET,
        BridgeConstants.TtsBackground.Action.UPDATE_PUNCTUATION_ECHO,
        punctuationEcho);
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
        BridgeConstants.UserActionMonitor.TARGET,
        BridgeConstants.UserActionMonitor.Action.CREATE, actions);
  },

  /**
   * Destroys the user action monitor.
   * @return {!Promise}
   */
  async destroy() {
    return BridgeHelper.sendMessage(
        BridgeConstants.UserActionMonitor.TARGET,
        BridgeConstants.UserActionMonitor.Action.DESTROY);
  },

  /** @return {!Promise<boolean>} */
  async onKeyDown(event) {
    return BridgeHelper.sendMessage(
        BridgeConstants.UserActionMonitor.TARGET,
        BridgeConstants.UserActionMonitor.Action.ON_KEY_DOWN, event);
  },
};
