// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides functions used for recording user actions within settings.
 * Also provides a way to inject a test implementation for verifying
 * user action recording.
 */

import {SettingChangeValue, UserActionRecorder, UserActionRecorderInterface} from './mojom-webui/search/user_action_recorder.mojom-webui.js';
import {Setting} from './mojom-webui/setting.mojom-webui.js';

let userActionRecorder: UserActionRecorderInterface|null = null;

export function setUserActionRecorderForTesting(
    testRecorder: UserActionRecorderInterface): void {
  userActionRecorder = testRecorder;
}

function getRecorder(): UserActionRecorderInterface {
  if (userActionRecorder) {
    return userActionRecorder;
  }

  userActionRecorder = UserActionRecorder.getRemote();
  return userActionRecorder;
}

export function recordPageFocus(): void {
  getRecorder().recordPageFocus();
}

export function recordPageBlur(): void {
  getRecorder().recordPageBlur();
}

export function recordClick(): void {
  getRecorder().recordClick();
}

export function recordNavigation(): void {
  getRecorder().recordNavigation();
}

export function recordSearch(): void {
  getRecorder().recordSearch();
}

/**
 * All new code should pass a value for |setting| and, if applicable, |value|.
 * The zero-parameter version of this function is reserved for
 * legacy code which has not yet been converted.
 * TODO(b/263414450): make |setting| non-optional when migration is complete.
 */
export function recordSettingChange(
    setting?: Setting, value?: SettingChangeValue) {
  if (setting === undefined) {
    getRecorder().recordSettingChange();
  } else {
    getRecorder().recordSettingChangeWithDetails(setting, value || null);
  }
}
