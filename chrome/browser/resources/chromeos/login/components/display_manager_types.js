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
/* #export */ let DisplayManagerScreenAttributes = {};

/**
 * True if device reset is allowed on the screen.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.resetAllowed;

/**
 * Possible types of UI.
 * @enum {string}
 */
/* #export */ const DISPLAY_TYPE = {
  UNKNOWN: 'unknown',
  OOBE: 'oobe',
  LOGIN: 'login',
  APP_LAUNCH_SPLASH: 'app-launch-splash',
  GAIA_SIGNIN: 'gaia-signin'
};

/**
 * Oobe UI state constants.
 * Used to control native UI elements.
 * Should be in sync with login_types.h
 * @enum {number}
 */
/* #export */ const OOBE_UI_STATE = {
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
  ENROLLMENT_CANCEL_ENABLED: 16,
  ENROLLMENT_SUCCESS: 17,
  THEME_SELECTION: 18,
  MARKETING_OPT_IN: 19,
};

// TODO(crbug.com/1229130) - Refactor/remove these constants.
/* #export */ const SCREEN_WELCOME = 'connect';
/* #export */ const SCREEN_OOBE_NETWORK = 'network-selection';
/* #export */ const SCREEN_OOBE_HID_DETECTION = 'hid-detection';
/* #export */ const SCREEN_OOBE_ENABLE_DEBUGGING = 'debugging';
/* #export */ const SCREEN_OOBE_UPDATE = 'oobe-update';
/* #export */ const SCREEN_OOBE_RESET = 'reset';
/* #export */ const SCREEN_OOBE_ENROLLMENT = 'enterprise-enrollment';
/* #export */ const SCREEN_OOBE_DEMO_SETUP = 'demo-setup';
/* #export */ const SCREEN_OOBE_DEMO_PREFERENCES = 'demo-preferences';
/* #export */ const SCREEN_OOBE_KIOSK_ENABLE = 'kiosk-enable';
/* #export */ const SCREEN_PACKAGED_LICENSE = 'packaged-license';
/* #export */ const SCREEN_GAIA_SIGNIN = 'gaia-signin';
/* #export */ const SCREEN_ERROR_MESSAGE = 'error-message';
/* #export */ const SCREEN_PASSWORD_CHANGED = 'gaia-password-changed';
/* #export */ const SCREEN_APP_LAUNCH_SPLASH = 'app-launch-splash';
/* #export */ const SCREEN_CONFIRM_PASSWORD = 'saml-confirm-password';
/* #export */ const SCREEN_FATAL_ERROR = 'fatal-error';
/* #export */ const SCREEN_KIOSK_ENABLE = 'kiosk-enable';
/* #export */ const SCREEN_TERMS_OF_SERVICE = 'terms-of-service';
/* #export */ const SCREEN_ARC_TERMS_OF_SERVICE = 'arc-tos';
/* #export */ const SCREEN_DEVICE_DISABLED = 'device-disabled';
/* #export */ const SCREEN_UPDATE_REQUIRED = 'update-required';
/* #export */ const SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE =
'ad-password-change';
/* #export */ const SCREEN_SYNC_CONSENT = 'sync-consent';
/* #export */ const SCREEN_FINGERPRINT_SETUP = 'fingerprint-setup';
// TODO(crbug.com/1261902): Remove.
/* #export */ const SCREEN_RECOMMEND_APPS_OLD = 'recommend-apps-old';
/* #export */ const SCREEN_RECOMMEND_APPS = 'recommend-apps';
/* #export */ const SCREEN_APP_DOWNLOADING = 'app-downloading';
/* #export */ const SCREEN_PIN_SETUP = 'pin-setup';
/* #export */ const SCREEN_MARKETING_OPT_IN = 'marketing-opt-in';

/* Accelerator identifiers.
 * Must be kept in sync with webui_accelerator_mapping.cc.
 */
/* #export */ const ACCELERATOR_CANCEL = 'cancel';
/* #export */ const ACCELERATOR_VERSION = 'version';
/* #export */ const ACCELERATOR_RESET = 'reset';
/* #export */ const ACCELERATOR_APP_LAUNCH_BAILOUT = 'app_launch_bailout';
/* #export */ const ACCELERATOR_APP_LAUNCH_NETWORK_CONFIG =
    'app_launch_network_config';

/* #export */ const USER_ACTION_ROLLBACK_TOGGLED = 'rollback-toggled';


/**
 * Group of screens (screen IDs) where factory-reset screen invocation is
 * available. Newer screens using Polymer use the attribute
 * `resetAllowed` in their `ready()` method.
 * @type Array<string>
 * @const
 */
/* #export */ var RESET_AVAILABLE_SCREEN_GROUP = [
  SCREEN_OOBE_NETWORK,
  SCREEN_GAIA_SIGNIN,
  SCREEN_KIOSK_ENABLE,
  SCREEN_ERROR_MESSAGE,
  SCREEN_PASSWORD_CHANGED,
  SCREEN_ARC_TERMS_OF_SERVICE,
  SCREEN_CONFIRM_PASSWORD,
  SCREEN_UPDATE_REQUIRED,
  SCREEN_SYNC_CONSENT,
  SCREEN_APP_DOWNLOADING,
  SCREEN_PIN_SETUP,
  SCREEN_MARKETING_OPT_IN,
];
