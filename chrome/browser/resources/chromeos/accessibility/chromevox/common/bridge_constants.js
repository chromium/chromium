// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enums for BridgeHelper functions.
 */
goog.provide('BridgeAction');
goog.provide('BridgeActions');
goog.provide('BridgeConstants');
goog.provide('BridgeTarget');
goog.provide('BridgeTargets');

/** @enum {string} */
BridgeTargets = {
  CHROMEVOX_BACKGROUND: 'ChromeVoxBackground',
  CHROMEVOX_PREFS: 'ChromeVoxPrefs',
  CHROMEVOX_STATE: 'ChromeVoxState',
  COMMAND_HANDLER: 'CommandHandler',
  EVENT_SOURCE_STATE: 'EventSourceState',
  EVENT_STREAM_LOGGER: 'EventStreamLogger',
  GESTURE_COMMAND_HANDLER: 'GestureCommandHandler',
  LOG_STORE: 'LogStore',
  PANEL: 'Panel',
  PANEL_BACKGROUND: 'PanelBackground',
  USER_ACTION_MONITOR: 'UserActionMonitor',
};
/**
 * The class that a message is being sent to.
 * @typedef {BridgeTargets|string}
 */
BridgeTarget;

BridgeConstants = {
  BrailleBackground: {
    /** @public {BridgeTarget} */
    TARGET: 'BrailleBackground',
    /** @enum {string} */
    Action: {
      BACK_TRANSLATE: 'backTranslate',
      REFRESH_BRAILLE_TABLE: 'refreshBrailleTable',
    },
  },

  BrailleCommandHandler: {
    /** @public {BridgeTarget} */
    TARGET: 'BrailleCommandHandler',
    /** @enum {string} */
    Action: {
      SET_ENABLED: 'setEnabled',
    },
  },
};

/**
 * @enum {string}
 */
BridgeActions = {
  ADD_MENU_ITEM: 'addMenuItem',
  CLEAR_CURRENT_RANGE: 'clearCurrentRange',
  CLEAR_LOG: 'clearLog',
  CREATE: 'create',
  CREATE_ALL_NODE_MENU_BACKGROUNDS: 'createAllNodeMenuBackgrounds',
  CREATE_NEW_I_SEARCH: 'createNewISearch',
  DESTROY: 'destroy',
  DESTROY_I_SEARCH: 'destroyISearch',
  FOCUS_TAB: 'focusTab',
  GET: 'get',
  GET_ACTIONS_FOR_CURRENT_NODE: 'getActionsForCurrentNode',
  GET_CURRENT_VOICE: 'getCurrentVoice',
  GET_LOGS: 'getLogs',
  GET_PREFS: 'getPrefs',
  GET_TAB_MENU_DATA: 'getTabMenuData',
  INCREMENTAL_SEARCH: 'incrementalSearch',
  NODE_MENU_CALLBACK: 'nodeMenuCallback',
  NOTIFY_EVENT_STREAM_FILTER_CHANGED: 'notifyEventStreamFilterChanged',
  ON_COMMAND: 'onCommand',
  ON_CURRENT_RANGE_CHANGED: 'onCurrentRangeChanged',
  PERFORM_CUSTOM_ACTION_ON_CURRENT_NODE: 'performCustomActionOnCurrentNode',
  PERFORM_STANDARD_ACTION_ON_CURRENT_NODE: 'performStandardActionOnCurrentNode',
  SET_ENABLED: 'setEnabled',
  SET_LOGGING_PREFS: 'setLoggingPrefs',
  SET_PREF: 'setPref',
  SET_RANGE_TO_I_SEARCH_NODE: 'setRangeToISearchNode',
  UPDATE_PUNCTUATION_ECHO: 'updatePunctuationEcho',
  WAIT_FOR_PANEL_COLLAPSE: 'waitForPanelCollapse',
};

/**
 * The action that the message is requesting be performed.
 * @typedef {BridgeActions |
 *           BridgeConstants.BrailleBackground.Action |
 *           BridgeConstants.BrailleCommandHandler.Action}
 */
BridgeAction;
