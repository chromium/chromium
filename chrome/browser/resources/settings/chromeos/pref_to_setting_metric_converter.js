/* Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview Helper class for converting a given settings pref to a pair of
 * setting ID and setting change value. Used to record metrics about changes
 * to pref-based settings.
 */

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js'
// #import '../constants/setting.mojom-lite.js';
// #import '../search/user_action_recorder.mojom-lite.js';
// clang-format on

/* #export */ class PrefToSettingMetricConverter {
  /**
   * @param {string} prefKey
   * @param {*} prefValue
   * @return {?{setting: !chromeos.settings.mojom.Setting, value:
   *     !chromeos.settings.mojom.SettingChangeValue}}
   */
  convertPrefToSettingMetric(prefKey, prefValue) {
    switch (prefKey) {
      // device_page/keyboard.js
      case 'settings.language.send_function_keys':
        return {
          setting: chromeos.settings.mojom.Setting.kKeyboardFunctionKeys,
          value: {boolValue: /** @type {boolean} */ (prefValue)}
        };

      // device_page/pointers.js
      case 'settings.touchpad.sensitivity2':
        console.log(prefValue);
        return {
          setting: chromeos.settings.mojom.Setting.kTouchpadSpeed,
          value: {intValue: /** @type {number} */ (prefValue)}
        };

      // os_privacy_page/os_privacy_page.js
      case 'cros.device.peripheral_data_access_enabled':
        return {
          setting:
              chromeos.settings.mojom.Setting.kPeripheralDataAccessProtection,
          value: {boolValue: /** @type {boolean} */ (prefValue)}
        };

      // pref to setting metric not implemented.
      default:
        return null;
    }
  }
}
