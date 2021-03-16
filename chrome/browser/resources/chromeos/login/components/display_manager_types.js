// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Closure compiler type definitions used by display_manager.js .
 */

/**
 * @typedef {{
 *   resetAllowed: (boolean|undefined),
 * }}
 */
var DisplayManagerScreenAttributes = {};

/**
 * True if device reset is allowed on the screen.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.resetAllowed;

/**
 * Possible types of UI.
 * @enum {string}
 */
var DISPLAY_TYPE = {
  UNKNOWN: 'unknown',
  OOBE: 'oobe',
  LOGIN: 'login',
  LOCK: 'lock',
  APP_LAUNCH_SPLASH: 'app-launch-splash',
  DESKTOP_USER_MANAGER: 'login-add-user',
  GAIA_SIGNIN: 'gaia-signin'
};

/**
 * Oobe UI state constants.
 * Used to control native UI elements.
 * Should be in sync with login_types.h
 * @enum {number}
 */
/* #export */ var OOBE_UI_STATE = {
  HIDDEN: 0, /* Any OOBE screen without specific state */
  GAIA_SIGNIN: 1,
  ACCOUNT_PICKER: 2,
  WRONG_HWID_WARNING: 3,
  DEPRECATED_SUPERVISED_USER_CREATION_FLOW: 4,
  SAML_PASSWORD_CONFIRM: 5,
  PASSWORD_CHANGED: 6,
  ENROLLMENT: 7,
  ERROR: 8,
  ONBOARDING: 9,
  BLOCKING: 10,
  KIOSK: 11,
  MIGRATION: 12,
  USER_CREATION: 15,
};

/**
 * Possible UI states of the error screen.
 * @enum {string}
 */
var ERROR_SCREEN_UI_STATE = {
  UNKNOWN: 'ui-state-unknown',
  UPDATE: 'ui-state-update',
  SIGNIN: 'ui-state-signin',
  KIOSK_MODE: 'ui-state-kiosk-mode',
  LOCAL_STATE_ERROR: 'ui-state-local-state-error',
  AUTO_ENROLLMENT_ERROR: 'ui-state-auto-enrollment-error',
  ROLLBACK_ERROR: 'ui-state-rollback-error',
  SUPERVISED_USER_CREATION_FLOW: 'ui-state-supervised',
};
