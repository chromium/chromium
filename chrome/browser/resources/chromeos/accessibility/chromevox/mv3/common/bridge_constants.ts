// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enums for BridgeHelper functions.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

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

  ChromeVoxState: {
    TARGET: 'ChromeVoxState',
    Action: {
      IS_LEARN_MODE_READY: 'isLearnModeReady',
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

  Offscreen: {
    TARGET: 'Offscreen',
    Action: {
      CHROMEVOX_READY: 'chromeVoxReady',
      EARCON_CANCEL_LOADING: 'earconCancelLoading',
      EARCON_CANCEL_PROGRESS: 'earconCancelProgress',
      EARCON_RESET_PAN: 'earconSesetPan',
      EARCON_SET_POSITION_FOR_RECT: 'earconSetPositionForRect',
      IMAGE_DATA_FROM_URL: 'imageDataFromUrl',
      LEARN_MODE_REGISTER_LISTENERS: 'learnModeRegisterListeners',
      LEARN_MODE_REMOVE_LISTENERS: 'learnModeRemoveListeners',
      LIBLOUIS_START_WORKER: 'libLouisStartWorker',
      LIBLOUIS_RPC: 'libLouisRPC',
      ON_CLIPBOARD_DATA_CHANGED: 'onClipboardDataChanged',
      PLAY_EARCON: 'playEarcon',
      SHOULD_SET_DEFAULT_VOICE: 'shouldSetDefaultVoice',
      SRE_MOVE: 'sreMove',
      SRE_WALK: 'sreWalk',
    },
  },

  OffscreenTest: {
    TARGET: 'OffscreenTest',
    Action: {
      RECORD_EARCONS: 'recordEarcons',
      REPORT_EARCONS: 'reportEarcons',
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
      BRAILLE_PAN_LEFT: 'braillePanLeft',
      BRAILLE_PAN_RIGHT: 'braillePanRight',
      DISABLE_ERROR_MSG: 'disableErroMsg',
      DISABLE_TUTORIAL_RESTART_NUDGES: 'disableTutorialRestartNudges',
      FIRE_MOCK_EVENT: 'fireMockEvent',
      FIRE_MOCK_QUERY: 'fireMockQuery',
      GET_ACTIVE_MENU_DATA: 'getActiveMenuData',
      GET_ACTIVE_SEARCH_MENU_DATA: 'getActiveSearchMenuData',
      GET_TUTORIAL_ACTIVE_LESSON_INDEX: 'getTutorialActiveLessonIndex',
      GET_TUTORIAL_ACTIVE_SCREEN: 'getTutorialActiveScreen',
      GET_TUTORIAL_INTERACTIVE_MODE: 'getTutorialInteractiveMode',
      GET_TUTORIAL_READY: 'getTutorialReady',
      GET_FORCED_ACTION_PATH_CREATED_COUNT: 'getForcedActionPathCreatedCount',
      GET_FORCED_ACTION_PATH_DESTROYED_COUNT:
          'getForcedActionPathDestroyedCount',
      GET_IS_FORCED_ACTION_PATH_ACTIVE: 'getIsForcedActionPathActive',
      GIVE_TUTORIAL_NUDGE: 'giveTutorialNudge',
      INITIALIZE_TUTORIAL_NUDGES: 'initializeTutorialNudges',
      PERFORM_ACTIVE_MENU_TEST: 'performActiveMenuTest',
      PERFORM_ADD_MENU_TEST: 'performAddMenuTest',
      PERFORM_ADD_NODE_MENU_TEST: 'performAddNodeMenuTest',
      PERFORM_ADVANCE_ACTIVE_MENU_BY_TEST: 'performAdvanceActiveMenuByTest',
      PERFORM_CLEAR_MENUS_TEST: 'performClearMenusTest',
      PERFORM_DENY_SIGNED_OUT_TEST: 'performDenySignedOutTest',
      PERFORM_FIND_ENABLED_MENU_INDEX_TEST: 'performFindEnabledMenuIndexText',
      PERFORM_GET_SORTED_KEY_BINDINGS_TEST: 'peformGetSortedKeyBindingsTest',
      PERFORM_ON_SEARCH_BAR_QUERY_TEST: 'performOnSearchBarQueryTest',
      REPLACE_MENU_MANAGER: 'replaceMenuManger',
      RESTART_TUTORIAL_NUDGES: 'restartTutorialNudges',
      SET_TUTORIAL_CURRICULUM: 'setTutorialCurriculum',
      SET_TUTORIAL_MEDIUM: 'setTutorialMedium',
      SHOW_TUTORIAL_LESSON: 'showTutorialLesson',
      SHOW_TUTORIAL_LESSON_MENU: 'showTutorialLessonMenu',
      SHOW_TUTORIAL_MAIN_MENU: 'showTutorialMainMenu',
      SHOW_TUTORIAL_NEXT_LESSON: 'showTutorialNextLesson',
      SWAP_FORCED_ACTION_PATH_METHODS: 'swapForcedActionPathMethods',
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

TestImportManager.exportForTesting(['BridgeConstants', BridgeConstants]);
