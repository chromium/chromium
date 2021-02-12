// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js'
// #import '../constants/setting.mojom-lite.js';
// #import '../search/user_action_recorder.mojom-lite.js';
// clang-format on

/**
 * @fileoverview
 * Provides functions used for recording user actions within settings.
 * Also provides a way to inject a test implementation for verifying
 * user action recording.
 */
cr.define('settings', function() {
  /** @type {?chromeos.settings.mojom.UserActionRecorderInterface} */
  let userActionRecorder = null;

  /**
   * @param {!chromeos.settings.mojom.UserActionRecorderInterface}
   *     testRecorder
   */
  /* #export */ function setUserActionRecorderForTesting(testRecorder) {
    userActionRecorder = testRecorder;
  }

  /**
   * @return {!chromeos.settings.mojom.UserActionRecorderInterface}
   */
  function getRecorder() {
    if (userActionRecorder) {
      return userActionRecorder;
    }

    userActionRecorder = chromeos.settings.mojom.UserActionRecorder.getRemote();
    return userActionRecorder;
  }

  /* #export */ function recordPageFocus() {
    getRecorder().recordPageFocus();
  }

  /* #export */ function recordPageBlur() {
    getRecorder().recordPageBlur();
  }

  /* #export */ function recordClick() {
    getRecorder().recordClick();
  }

  /* #export */ function recordNavigation() {
    getRecorder().recordNavigation();
  }

  /* #export */ function recordSearch() {
    getRecorder().recordSearch();
  }

  /**
   * All new code should pass a value for |opt_setting| and, if applicable,
   * |opt_value|. The zero-parameter version of this function is reserved for
   * legacy code which has not yet been converted.
   * TODO(https://crbug.com/1133553): make |opt_setting| non-optional when
   * migration is complete.
   * @param {!chromeos.settings.mojom.Setting=} opt_setting
   * @param {!chromeos.settings.mojom.SettingChangeValue=} opt_value
   */
  /* #export */ function recordSettingChange(opt_setting, opt_value) {
    if (opt_setting === undefined) {
      getRecorder().recordSettingChange();
    } else {
      getRecorder().recordSettingChangeWithDetails(
          opt_setting, opt_value || null);
    }
  }

  // #cr_define_end
  return {
    setUserActionRecorderForTesting,
    recordPageFocus,
    recordPageBlur,
    recordClick,
    recordNavigation,
    recordSearch,
    recordSettingChange,
  };
});
