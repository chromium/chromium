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
  EVENT_STREAM_LOGGER: 'EventStreamLogger',
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

  ChromeVoxBackground: {
    /** @public {BridgeTarget} */
    TARGET: 'ChromeVoxBackground',
    /** @enum {string} */
    Action: {
      GET_CURRENT_VOICE: 'getCurrentVoice',
    },
  },

  ChromeVoxPrefs: {
    /** @public {BridgeTarget} */
    TARGET: 'ChromeVoxPrefs',
    /** @enum {string} */
    Action: {
      GET_PREFS: 'getPrefs',
      SET_LOGGING_PREFS: 'setLoggingPrefs',
      SET_PREF: 'setPref',
    },
  },

  ChromeVoxState: {
    /** @public {BridgeTarget} */
    TARGET: 'ChromeVoxState',
    /** @enum {string} */
    Action: {
      CLEAR_CURRENT_RANGE: 'clearCurrentRange',
      UPDATE_PUNCTUATION_ECHO: 'updatePunctuationEcho',
    },
  },

  CommandHandler: {
    /** @public {BridgeTarget} */
    TARGET: 'CommandHandler',
    /** @enum {string} */
    Action: {
      ON_COMMAND: 'onCommand',
    },
  },

  EventSourceState: {
    /** @public {BridgeTarget} */
    TARGET: 'EventSourceState',
    /** @enum {string} */
    Action: {
      GET: 'get',
    },
  },

  GestureCommandHandler: {
    /** @public {BridgeTarget} */
    TARGET: 'GestureCommandHandler',
    /** @enum {string} */
    Action: {
      SET_ENABLED: 'setEnabled',
    },
  },

  LogStore: {
    /** @public {BridgeTarget} */
    TARGET: 'LogStore',
    /** @enum {string} */
    Action: {
      CLEAR_LOG: 'clearLog',
      GET_LOGS: 'getLogs',
    },
  },

  Panel: {
    /** @public {BridgeTarget} */
    TARGET: 'Panel',
    /** @enum {string} */
    Action: {
      ADD_MENU_ITEM: 'addMenuItem',
      ON_CURRENT_RANGE_CHANGED: 'onCurrentRangeChanged',
    },
  },
};

/**
 * @enum {string}
 */
BridgeActions = {
  CLEAR_SAVED_NODE: 'clearSavedNode',
  CREATE: 'create',
  CREATE_ALL_NODE_MENU_BACKGROUNDS: 'createAllNodeMenuBackgrounds',
  CREATE_NEW_I_SEARCH: 'createNewISearch',
  DESTROY: 'destroy',
  DESTROY_I_SEARCH: 'destroyISearch',
  FOCUS_TAB: 'focusTab',
  GET_ACTIONS_FOR_CURRENT_NODE: 'getActionsForCurrentNode',
  GET_TAB_MENU_DATA: 'getTabMenuData',
  INCREMENTAL_SEARCH: 'incrementalSearch',
  NODE_MENU_CALLBACK: 'nodeMenuCallback',
  NOTIFY_EVENT_STREAM_FILTER_CHANGED: 'notifyEventStreamFilterChanged',
  PERFORM_CUSTOM_ACTION_ON_CURRENT_NODE: 'performCustomActionOnCurrentNode',
  PERFORM_STANDARD_ACTION_ON_CURRENT_NODE: 'performStandardActionOnCurrentNode',
  SAVE_CURRENT_NODE: 'saveCurrentNode',
  SET_RANGE_TO_I_SEARCH_NODE: 'setRangeToISearchNode',
  WAIT_FOR_PANEL_COLLAPSE: 'waitForPanelCollapse',
};

/**
 * The action that the message is requesting be performed.
 * @typedef {BridgeActions |
 *           BridgeConstants.BrailleBackground.Action |
 *           BridgeConstants.BrailleCommandHandler.Action |
 *           BridgeConstants.ChromeVoxBackground.Action |
 *           BridgeConstants.ChromeVoxPrefs.Action |
 *           BridgeConstants.ChromeVoxState.Action |
 *           BridgeConstants.CommandHandler.Action |
 *           BridgeConstants.EventSourceState.Action |
 *           BridgeConstants.GestureCommandHandler.Action |
 *           BridgeConstants.LogStore.Action |
 *           BridgeConstants.Panel.Action}
 */
BridgeAction;
