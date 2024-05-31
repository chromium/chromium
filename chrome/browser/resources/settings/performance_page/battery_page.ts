// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ControlledRadioButtonElement} from '../controls/controlled_radio_button.js';
import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './battery_page.html.js';
import type {PerformanceMetricsProxy} from './performance_metrics_proxy.js';
import {BatterySaverModeState, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';

export const BATTERY_SAVER_MODE_PREF =
    'performance_tuning.battery_saver_mode.state';

export interface SettingsBatteryPageElement {
  $: {
    enabledOnBatteryButton: ControlledRadioButtonElement,
    radioGroup: SettingsRadioGroupElement,
    toggleButton: SettingsToggleButtonElement,
  };
}

const SettingsBatteryPageElementBase = PrefsMixin(PolymerElement);

export class SettingsBatteryPageElement extends SettingsBatteryPageElementBase {
  static get is() {
    return 'settings-battery-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      batterySaverModeStateEnum_: {
        readOnly: true,
        type: Object,
        value: BatterySaverModeState,
      },

      isBatterySaverModeManagedByOS_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isBatterySaverModeManagedByOS');
        },
      },

      numericUncheckedValues_: {
        type: Array,
        value: () => [BatterySaverModeState.DISABLED],
      },
    };
  }

  private numericUncheckedValues_: BatterySaverModeState[];
  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private isBatterySaverModeEnabled_(value: number): boolean {
    return value !== BatterySaverModeState.DISABLED;
  }

  private onChange_() {
    this.metricsProxy_.recordBatterySaverModeChanged(
        this.getPref<number>(BATTERY_SAVER_MODE_PREF).value);
  }

  // <if expr="is_chromeos">
  private openOsPowerSettings_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osPowerSettingsUrl'));
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-battery-page': SettingsBatteryPageElement;
  }
}

customElements.define(
    SettingsBatteryPageElement.is, SettingsBatteryPageElement);
