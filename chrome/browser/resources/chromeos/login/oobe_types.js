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
 *   code: (String|undefined),
 *   displayName: (String|undefined),
 *   textDirection: (String|undefined),
 *   nativeDisplayName: (String|undefined),
 * }}
 */
OobeTypes.LanguageDsc;

/**
 * ChromeOS OOBE input method descriptor.
 * @typedef {{
 *   value: (String|undefined),
 *   title: (String|undefined),
 *   selected: (Boolean|undefined),
 * }}
 */
OobeTypes.IMEDsc;

/**
 * A set of flags of accessibility options for ChromeOS OOBE.
 * @typedef {{
 *   highContrastEnabled: Boolean,
 *   spokenFeedbackEnabled: Boolean,
 *   screenMagnifierEnabled: Boolean,
 *   largeCursorEnabled: Boolean,
 *   virtualKeyboardEnabled: Boolean,
 * }}
 */
OobeTypes.A11yStatuses;

/**
 * OOBE screen object (which is created in ui/login/screen.js )
 * @typedef {!Object}
 */
OobeTypes.Screen;

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
 *   selected: (Boolean|undefined),
 * }}
 */
OobeTypes.TimezoneDsc;

/**
 * OOBE configuration, allows automation during OOBE.
 * Keys are also listed in chrome/browser/chromeos/login/configuration_keys.h
 * @typedef {{
 *   welcomeNext: boolean|undefined,
 *   enableDemoMode: boolean|undefined,
 *   demoPreferencesNext: boolean|undefined,
 *   networkSelectGuid: string|undefined,
 *   networkOfflineDemo: boolean|undefined,
 *   eulaAutoAccept: boolean|undefined,
 *   eulaSendStatistics: boolean|undefined,
 *   updateSkipNonCritical: boolean|undefined,
 *   arcTosAutoAccept: boolean|undefined,
 * }}
 */
OobeTypes.OobeConfiguration;
