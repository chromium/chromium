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
};

/**
 * The action that the message is requesting be performed.
 * @enum {string}
 */
BridgeAction = {
  BACK_TRANSLATE: 'backTranslate',
  GET_PREFS: 'getPrefs',
  REFRESH_BRAILLE_TABLE: 'refreshBrailleTable',
  SET_LOGGING_PREFS: 'setLoggingPrefs',
  SET_PREF: 'setPref',
  UPDATE_PUNCTUATION_ECHO: 'updatePunctuationEcho',
};
