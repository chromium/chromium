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
let DisplayManagerScreenAttributes = {};

/**
 * True if device reset is allowed on the screen.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.resetAllowed;

/**
 * Possible types of UI.
 * @enum {string}
 */
const DISPLAY_TYPE = {
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
};

/**
 * Possible UI states of the error screen.
 * @enum {string}
 */
const ERROR_SCREEN_UI_STATE = {
  UNKNOWN: 'ui-state-unknown',
  UPDATE: 'ui-state-update',
  SIGNIN: 'ui-state-signin',
  KIOSK_MODE: 'ui-state-kiosk-mode',
  LOCAL_STATE_ERROR: 'ui-state-local-state-error',
  AUTO_ENROLLMENT_ERROR: 'ui-state-auto-enrollment-error',
  ROLLBACK_ERROR: 'ui-state-rollback-error',
  SUPERVISED_USER_CREATION_FLOW: 'ui-state-supervised',
};

// TODO(crbug.com/1229130) - Refactor/remove these constants.
const SCREEN_WELCOME = 'connect';
const SCREEN_OOBE_NETWORK = 'network-selection';
const SCREEN_OOBE_HID_DETECTION = 'hid-detection';
const SCREEN_OOBE_ENABLE_DEBUGGING = 'debugging';
const SCREEN_OOBE_UPDATE = 'oobe-update';
const SCREEN_OOBE_RESET = 'reset';
const SCREEN_OOBE_ENROLLMENT = 'enterprise-enrollment';
const SCREEN_OOBE_DEMO_SETUP = 'demo-setup';
const SCREEN_OOBE_DEMO_PREFERENCES = 'demo-preferences';
const SCREEN_OOBE_KIOSK_ENABLE = 'kiosk-enable';
const SCREEN_PACKAGED_LICENSE = 'packaged-license';
const SCREEN_GAIA_SIGNIN = 'gaia-signin';
const SCREEN_ERROR_MESSAGE = 'error-message';
const SCREEN_PASSWORD_CHANGED = 'gaia-password-changed';
const SCREEN_APP_LAUNCH_SPLASH = 'app-launch-splash';
const SCREEN_CONFIRM_PASSWORD = 'saml-confirm-password';
const SCREEN_FATAL_ERROR = 'fatal-error';
const SCREEN_KIOSK_ENABLE = 'kiosk-enable';
const SCREEN_TERMS_OF_SERVICE = 'terms-of-service';
const SCREEN_ARC_TERMS_OF_SERVICE = 'arc-tos';
const SCREEN_DEVICE_DISABLED = 'device-disabled';
const SCREEN_UPDATE_REQUIRED = 'update-required';
const SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE =
'ad-password-change';
const SCREEN_SYNC_CONSENT = 'sync-consent';
const SCREEN_FINGERPRINT_SETUP = 'fingerprint-setup';
const SCREEN_RECOMMEND_APPS = 'recommend-apps';
const SCREEN_APP_DOWNLOADING = 'app-downloading';
const SCREEN_PIN_SETUP = 'pin-setup';
const SCREEN_MARKETING_OPT_IN = 'marketing-opt-in';

/* Accelerator identifiers.
 * Must be kept in sync with webui_accelerator_mapping.cc.
 */
const ACCELERATOR_CANCEL = 'cancel';
const ACCELERATOR_VERSION = 'version';
const ACCELERATOR_RESET = 'reset';
const ACCELERATOR_APP_LAUNCH_BAILOUT = 'app_launch_bailout';
const ACCELERATOR_APP_LAUNCH_NETWORK_CONFIG =
    'app_launch_network_config';

const USER_ACTION_ROLLBACK_TOGGLED = 'rollback-toggled';
