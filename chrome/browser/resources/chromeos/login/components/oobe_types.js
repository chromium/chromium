// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * This file contains typedefs for chromeOS OOBE properties.
 */

var OobeTypes = {};

/**
 * ChromeOS OOBE language descriptor.
 * @typedef {{
 *   code: (string|undefined),
 *   displayName: (string|undefined),
 *   nativeDisplayName: (string|undefined),
 *   optionGroupName: (string|undefined),
 *   selected: boolean,
 *   textDirection: (string|undefined),
 *   title: string,
 *   value: string,
 * }}
 */
OobeTypes.LanguageDsc;

/**
 * ChromeOS OOBE input method descriptor.
 * @typedef {{
 *   optionGroupName: (string|undefined),
 *   selected: boolean,
 *   title: string,
 *   value: string,
 * }}
 */
OobeTypes.IMEDsc;

/**
 * ChromeOS OOBE demo country descriptor.
 * @typedef {{
 *   value: !string,
 *   title: (string|undefined),
 *   selected: (boolean|undefined),
 * }}
 */
OobeTypes.DemoCountryDsc;

/**
 * A set of flags of accessibility options for ChromeOS OOBE.
 * @typedef {{
 *   highContrastEnabled: boolean,
 *   spokenFeedbackEnabled: boolean,
 *   screenMagnifierEnabled: boolean,
 *   largeCursorEnabled: boolean,
 *   virtualKeyboardEnabled: boolean,
 * }}
 */
OobeTypes.A11yStatuses;

/**
 * Timezone ID.
 * @typedef {!String}
 */
OobeTypes.Timezone;

/**
 * ChromeOS timezone descriptor.
 * @typedef {{
 *   value: (OobeTypes.Timezone|undefined),
 *   title: (String|undefined),
 *   selected: (boolean|undefined),
 * }}
 */
OobeTypes.TimezoneDsc;

/**
 * OOBE configuration, allows automation during OOBE.
 * Keys are also listed in chrome/browser/ash/login/configuration_keys.h
 * @typedef {{
 *   language: (string|undefined),
 *   inputMethod: (string|undefined),
 *   welcomeNext: (boolean|undefined),
 *   enableDemoMode: (boolean|undefined),
 *   demoPreferencesNext: (boolean|undefined),
 *   networkSelectGuid: (string|undefined),
 *   networkOfflineDemo: (boolean|undefined),
 *   eulaAutoAccept: (boolean|undefined),
 *   eulaSendStatistics: (boolean|undefined),
 *   networkUseConnected: (boolean|undefined),
 *   arcTosAutoAccept: (boolean|undefined),
 * }}
 */
OobeTypes.OobeConfiguration;

/**
 * Parameters passed to show PIN setup screen
 * @typedef {{
 *   auth_token: string,
 * }}
 */
OobeTypes.PinSetupScreenParameters;

/**
 * Configuration of the security token PIN dialog.
 * @typedef {{
 *   enableUserInput: boolean,
 *   attemptsLeft: number,
 *   hasError: boolean,
 *   formattedError: string,
 *   formattedAttemptsLeft: string,
 * }}
 */
OobeTypes.SecurityTokenPinDialogParameters;

/**
 * Event sent from inner webview to enclosing Recommended apps screen.
 * @typedef {{
 *   type: (string|undefined),
 *   numOfSelected: number,
 * }}
 */
OobeTypes.RecommendedAppsSelectionEventData;

/**
 * Specifies the mechanism for calculating oobe-dialog inner padding.
 * @enum {string}
 */
OobeTypes.DialogPaddingMode = {
  AUTO: 'auto',
  NARROW: 'narrow',
  WIDE: 'wide',
};

/**
 * Fatal Error Codes from SignInFatalErrorScreen
 * @enum {number}
 */
OobeTypes.FatalErrorCode = {
  UNKNOWN: 0,
  SCRAPED_PASSWORD_VERIFICATION_FAILURE: 1,
  INSECURE_CONTENT_BLOCKED: 2,
  MISSING_GAIA_INFO: 3,
};
