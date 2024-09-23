// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides functions used for recording user actions within settings.
 * Also provides a way to inject a test implementation for verifying
 * user action recording.
 */

import {Setting} from './mojom-webui/setting.mojom-webui.js';
import {SettingChangeValue, UserActionRecorder, UserActionRecorderInterface} from './mojom-webui/user_action_recorder.mojom-webui.js';

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
 * This function is reserved only for pref-based setting changes that have no
 * corresponding entry in `metrics_utils.ts`. This function should not be used
 * in any new code.
 */
export function recordSettingChangeForUnmappedPref(): void {
  getRecorder().recordSettingChange();
}

/**
 * Records when a `setting` is changed and, if applicable, its updated `value`.
 */
export function recordSettingChange(
    setting: Setting, value?: SettingChangeValue): void {
  getRecorder().recordSettingChangeWithDetails(setting, value || null);
}
