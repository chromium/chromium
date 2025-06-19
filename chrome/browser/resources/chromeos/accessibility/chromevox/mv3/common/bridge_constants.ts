// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enums for BridgeHelper functions.
 */

/**
 * Specifies one of the renderer contexts for the ChromeVox extension. Code
 * specific to each of these contexts is contained in the corresponding
 * directory, while code used by two or more contexts is found in common/.
 * @enum {string}
 */
export enum BridgeContext {
  BACKGROUND = 'background',
  LEARN_MODE = 'learnMode',
  LOG_PAGE = 'logPage',
  OPTIONS = 'options',
  PANEL = 'panel',
}

export const BridgeConstants = {
  BackgroundKeyboardHandler: {
    TARGET: 'BackgroundKeyboardHandler',
    Action: {
      ON_KEY_DOWN: 'onKeyDown',
      ON_KEY_UP: 'onKeyUp',
    },
  },

  Braille: {
    TARGET: 'Braille',
    Action: {
      BACK_TRANSLATE: 'backTranslate',
      PAN_LEFT: 'panLeft',
      PAN_RIGHT: 'panRight',
      SET_BYPASS: 'setBypass',
      WRITE: 'write',
    },
  },

  BrailleBackground: {
    TARGET: 'BrailleBackground',
    Action: {
      BRAILLE_ROUTE: 'brailleRoute',
    },
  },

  ChromeVoxPrefs: {
    TARGET: 'ChromeVoxPrefs',
    Action: {
      GET_PREFS: 'getPrefs',
      GET_STICKY_PREF: 'getStickyPref',
      SET_LOGGING_PREFS: 'setLoggingPrefs',
      SET_PREF: 'setPref',
    },
  },

  ChromeVoxRange: {
    TARGET: 'ChromeVoxRange',
    Action: {
      CLEAR_CURRENT_RANGE: 'clearCurrentRange',
    },
  },

  CommandHandler: {
    TARGET: 'CommandHandler',
    Action: {
      ON_COMMAND: 'onCommand',
    },
  },

  Earcons: {
    TARGET: 'Earcons',
    Action: {
      CANCEL_EARCON: 'cancelEarcon',
      PLAY_EARCON: 'playEarcon',
    },
  },

  EventSource: {
    TARGET: 'EventSource',
    Action: {
      GET: 'get',
    },
  },

  EventStreamLogger: {
    TARGET: 'EventStreamLogger',
    Action: {
      NOTIFY_EVENT_STREAM_FILTER_CHANGED: 'notifyEventStreamFilterChanged',
    },
  },

  ForcedActionPath: {
    TARGET: 'ForcedActionPath',
    Action: {
      LISTEN_FOR: 'listenFor',
      ON_KEY_DOWN: 'onKeyDown',
      STOP_LISTENING: 'stopListening',
    },
  },

  GestureCommandHandler: {
    TARGET: 'GestureCommandHandler',
    Action: {
      SET_BYPASS: 'setBypass',
    },
  },

  LearnMode: {
    TARGET: 'LearnMode',
    Action: {
      ON_KEY_DOWN: 'onKeyDown',
      ON_KEY_UP: 'onKeyUp',
      ON_KEY_PRESS: 'onKeyPress',
    },
  },

  LearnModeTest: {
    TARGET: 'LearnModeTest',
    Action: {
      CLEAR_TOUCH_EXPLORE_OUTPUT_TIME: 'clearTouchExploreOutputTime',
      ON_ACCESSIBILITY_GESTURE: 'onAccessibilityGesture',
      ON_BRAILLE_KEY_EVENT: 'onBrailleKeyEvent',
      READY: 'ready',
    },
  },

  LibLouis: {
    TARGET: 'LibLouis',
    Action: {
      MESSAGE: 'message',
      ERROR: 'error',
    },
  },

  LocaleOutputHelper: {
    TARGET: 'LocaleOutputHelper',
    Action: {ON_VOICES_CHANGED: 'onVoicesChanged'},
  },

  LogStore: {
    TARGET: 'LogStore',
    Action: {
      CLEAR_LOG: 'clearLog',
      GET_LOGS: 'getLogs',
    },
  },

  Panel: {
    TARGET: 'Panel',
    Action: {
      IS_PANEL_INITIALIZED: 'IsPanelInitialized',
      EXEC_COMMAND: 'execCommand',
      ADD_MENU_ITEM: 'addMenuItem',
      ON_CURRENT_RANGE_CHANGED: 'onCurrentRangeChanged',
    },
  },

  PanelTest: {
    TARGET: 'PanelTest',
    Action: {
      BRAILLE_PAN_RIGHT: 'braille_pan_right',
      BRAILLE_PAN_LEFT: 'braille_pan_left',
      DISABLE_ERROR_MSG: 'disable_error_msg',
      FIRE_MOCK_EVENT: 'fire_mock_event',
      FIRE_MOCK_QUERY: 'fire_mock_query',
      GET_ACTIVE_MENU_DATA: 'get_active_menu_data',
      GET_ACTIVE_SEARCH_MENU_DATA: 'get_active_search_menu_data'
    },
  },

  PanelBackground: {
    TARGET: 'PanelBackground',
    Action: {
      CLEAR_SAVED_NODE: 'clearSavedNode',
      CREATE_ALL_NODE_MENU_BACKGROUNDS: 'createAllNodeMenuBackgrounds',
      CREATE_NEW_I_SEARCH: 'createNewISearch',
      DESTROY_I_SEARCH: 'destroyISearch',
      GET_ACTIONS_FOR_CURRENT_NODE: 'getActionsForCurrentNode',
      INCREMENTAL_SEARCH: 'incrementalSearch',
      NODE_MENU_CALLBACK: 'nodeMenuCallback',
      ON_TUTORIAL_READY: 'onTutorialReady',
      PERFORM_CUSTOM_ACTION_ON_CURRENT_NODE: 'performCustomActionOnCurrentNode',
      PERFORM_STANDARD_ACTION_ON_CURRENT_NODE:
          'performStandardActionOnCurrentNode',
      SAVE_CURRENT_NODE: 'saveCurrentNode',
      SET_PANEL_COLLAPSE_WATCHER: 'setPanelCollapseWatcher',
      SET_RANGE_TO_I_SEARCH_NODE: 'setRangeToISearchNode',
      WAIT_FOR_PANEL_COLLAPSE: 'waitForPanelCollapse',
    },
  },

  PrimaryTts: {
    TARGET: 'PrimaryTts',
    Action: {ON_VOICES_CHANGED: 'onVoicesChanged'},
  },

  TtsBackground: {
    TARGET: 'TtsBackground',
    Action: {
      GET_CURRENT_VOICE: 'getCurrentVoice',
      SPEAK: 'speak',
      UPDATE_PUNCTUATION_ECHO: 'updatePunctuationEcho',
    },
  },
};
