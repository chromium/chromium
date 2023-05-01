// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '/shared/settings/controls/controlled_radio_button.js';
import '/shared/settings/controls/settings_radio_group.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {ControlledRadioButtonElement} from '/shared/settings/controls/controlled_radio_button.js';
import {SettingsRadioGroupElement} from '/shared/settings/controls/settings_radio_group.js';
import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './battery_page.html.js';
import {BatterySaverModeState, PerformanceMetricsProxy, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';

export const BATTERY_SAVER_MODE_PREF =
    'performance_tuning.battery_saver_mode.state';

export interface SettingsBatteryPageElement {
  $: {
    enabledOnBatteryButton: ControlledRadioButtonElement,
    radioGroup: SettingsRadioGroupElement,
    radioGroupCollapse: IronCollapseElement,
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
    };
  }

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private isBatterySaverModeEnabled_(value: number): boolean {
    return value !== BatterySaverModeState.DISABLED;
  }

  private onChange_() {
    this.metricsProxy_.recordBatterySaverModeChanged(
        this.getPref<number>(BATTERY_SAVER_MODE_PREF).value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-battery-page': SettingsBatteryPageElement;
  }
}

customElements.define(
    SettingsBatteryPageElement.is, SettingsBatteryPageElement);
