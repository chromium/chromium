// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enums for BridgeHelper functions.
 */
goog.provide('BridgeAction');
goog.provide('BridgeTarget');

/**
 * The class that a message is being sent to.
 * @enum {string}
 */
BridgeTarget = {
  BRAILLE_BACKGROUND: 'BrailleBackground',
  CHROMEVOX_PREFS: 'ChromeVoxPrefs',
  CHROMEVOX_STATE: 'ChromeVoxState',
  COMMAND_HANDLER: 'CommandHandler',
  EVENT_SOURCE_STATE: 'EventSourceState',
  LOG_STORE: 'LogStore',
  PANEL_BACKGROUND: 'PanelBackground',
  PANEL: 'Panel',
};

/**
 * The action that the message is requesting be performed.
 * @enum {string}
 */
BridgeAction = {
  BACK_TRANSLATE: 'backTranslate',
  CLEAR_LOG: 'clearLog',
  CREATE_NEW_I_SEARCH: 'createNewISearch',
  DESTROY_I_SEARCH: 'destroyISearch',
  GET: 'get',
  GET_ACTIONS_FOR_CURRENT_NODE: 'getActionsForCurrentNode',
  GET_LOGS: 'getLogs',
  GET_PREFS: 'getPrefs',
  INCREMENTAL_SEARCH: 'incrementalSearch',
  ON_COMMAND: 'onCommand',
  ON_CURRENT_RANGE_CHANGED: 'onCurrentRangeChanged',
  PERFORM_CUSTOM_ACTION_ON_CURRENT_NODE: 'performCustomActionOnCurrentNode',
  PERFORM_STANDARD_ACTION_ON_CURRENT_NODE: 'performStandardActionOnCurrentNode',
  REFRESH_BRAILLE_TABLE: 'refreshBrailleTable',
  SET_LOGGING_PREFS: 'setLoggingPrefs',
  SET_PREF: 'setPref',
  SET_RANGE_TO_I_SEARCH_NODE: 'setRangeToISearchNode',
  UPDATE_PUNCTUATION_ECHO: 'updatePunctuationEcho',
  WAIT_FOR_PANEL_COLLAPSE: 'waitForPanelCollapse',
};
