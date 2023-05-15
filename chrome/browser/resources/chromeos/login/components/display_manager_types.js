// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Closure compiler type definitions used by display_manager.js .
 */

/**
 * Possible types of UI.
 * @enum {string}
 */
export const DISPLAY_TYPE = {
  UNKNOWN: 'unknown',
  OOBE: 'oobe',
  LOGIN: 'login',
  APP_LAUNCH_SPLASH: 'app-launch-splash',
  GAIA_SIGNIN: 'gaia-signin',
};

/**
 * Oobe UI state constants.
 * Used to control native UI elements.
 * Should be in sync with login_types.h
 * @enum {number}
 */
export const OOBE_UI_STATE = {
  HIDDEN: 0, /* Any OOBE screen without specific state */
  GAIA_SIGNIN: 1,
  ACCOUNT_PICKER: 2,
  WRONG_HWID_WARNING: 3,
  DEPRECATED_SUPERVISED_USER_CREATION_FLOW: 4,
  SAML_PASSWORD_CONFIRM: 5,
  PASSWORD_CHANGED: 6,
  ENROLLMENT_CANCEL_DISABLED: 7,
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
  GAIA_INFO: 21,
};

// TODO(crbug.com/1229130) - Refactor/remove these constants.
export const SCREEN_WELCOME = 'connect';
export const SCREEN_GAIA_SIGNIN = 'gaia-signin';
export const SCREEN_DEVICE_DISABLED = 'device-disabled';

/* Accelerator identifiers.
 * Must be kept in sync with webui_accelerator_mapping.cc.
 */
export const ACCELERATOR_CANCEL = 'cancel';
