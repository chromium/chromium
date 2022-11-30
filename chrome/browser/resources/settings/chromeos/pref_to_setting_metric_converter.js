/* Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview Helper class for converting a given settings pref to a pair of
 * setting ID and setting change value. Used to record metrics about changes
 * to pref-based settings.
 */

import {SettingChangeValue} from '../mojom-webui/search/user_action_recorder.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

export class PrefToSettingMetricConverter {
  /**
   * @param {string} prefKey
   * @param {*} prefValue
   * @return {?{setting: !Setting, value: !SettingChangeValue}}
   */
  convertPrefToSettingMetric(prefKey, prefValue) {
    switch (prefKey) {
      // device_page/keyboard.js
      case 'settings.language.send_function_keys':
        return {
          setting: Setting.kKeyboardFunctionKeys,
          value: {boolValue: /** @type {boolean} */ (prefValue)},
        };

      // device_page/pointers.js
      case 'settings.touchpad.sensitivity2':
        return {
          setting: Setting.kTouchpadSpeed,
          value: {intValue: /** @type {number} */ (prefValue)},
        };

      // os_privacy_page/os_privacy_page.js
      case 'cros.device.peripheral_data_access_enabled':
        return {
          setting: Setting.kPeripheralDataAccessProtection,
          value: {boolValue: /** @type {boolean} */ (prefValue)},
        };

      // pref to setting metric not implemented.
      default:
        return null;
    }
  }
}
