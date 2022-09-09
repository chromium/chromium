// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides functions used for recording user actions within settings.
 * Also provides a way to inject a test implementation for verifying
 * user action recording.
 */

import {SettingChangeValue, UserActionRecorder, UserActionRecorderInterface} from '../mojom-webui/search/user_action_recorder.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

/** @type {?UserActionRecorderInterface} */
let userActionRecorder = null;

/**
 * @param {!UserActionRecorderInterface} testRecorder
 */
export function setUserActionRecorderForTesting(testRecorder) {
  userActionRecorder = testRecorder;
}

/**
 * @return {!UserActionRecorderInterface}
 */
function getRecorder() {
  if (userActionRecorder) {
    return userActionRecorder;
  }

  userActionRecorder = UserActionRecorder.getRemote();
  return userActionRecorder;
}

export function recordPageFocus() {
  getRecorder().recordPageFocus();
}

export function recordPageBlur() {
  getRecorder().recordPageBlur();
}

export function recordClick() {
  getRecorder().recordClick();
}

export function recordNavigation() {
  getRecorder().recordNavigation();
}

export function recordSearch() {
  getRecorder().recordSearch();
}

/**
 * All new code should pass a value for |opt_setting| and, if applicable,
 * |opt_value|. The zero-parameter version of this function is reserved for
 * legacy code which has not yet been converted.
 * TODO(https://crbug.com/1133553): make |opt_setting| non-optional when
 * migration is complete.
 * @param {!Setting=} opt_setting
 * @param {!SettingChangeValue=} opt_value
 */
export function recordSettingChange(opt_setting, opt_value) {
  if (opt_setting === undefined) {
    getRecorder().recordSettingChange();
  } else {
    getRecorder().recordSettingChangeWithDetails(
        opt_setting, opt_value || null);
  }
}
