// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';

import {getTemplate} from './battery_page.html.js';
import {PerformanceBrowserProxy, PerformanceBrowserProxyImpl} from './performance_browser_proxy.js';
// clang-format on

export interface SettingsBatteryPageElement {
  $: {
    radioGroup: SettingsRadioGroupElement,
    radioGroupCollapse: IronCollapseElement,
    toggleButton: SettingsToggleButtonElement,
  };
}

export class SettingsBatteryPageElement extends PolymerElement {
  static get is() {
    return 'settings-battery-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Possible values for the
       * 'prefs.performance_tuning.battery_saver_mode.state' preference. These
       * values map to
       * performance_manager::user_tuning::prefs::BatterySaverModeState, and
       * are written to prefs and metrics, so order should not be changed.
       */
      batterySaverModeStatePrefValues: {
        readOnly: true,
        type: Object,
        value: {
          disabled: 0,
          enabledBelowThreshold: 1,
          enabledOnBattery: 2,
          enabled: 3,
        },
      },
    };
  }

  batterySaverModeStatePrefValues: {
    disabled: number,
    enabledBelowThreshold: number,
    enabledOnBattery: number,
    enabled: number,
  };

  private browserProxy_: PerformanceBrowserProxy =
      PerformanceBrowserProxyImpl.getInstance();

  private isBatterySaverModeEnabled_(value: number): boolean {
    return value !== this.batterySaverModeStatePrefValues.disabled;
  }

  private onLearnMoreOrSendFeedbackClick_(e: CustomEvent<string>) {
    switch (e.detail) {
      case 'batterySaverLearnMore':
        OpenWindowProxyImpl.getInstance().openURL(
            loadTimeData.getString('batterySaverLearnMoreUrl'));
        break;
      case 'batterySaverSendFeedback':
        this.browserProxy_.openBatterySaverFeedbackDialog();
        break;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-battery-page': SettingsBatteryPageElement;
  }
}

customElements.define(
    SettingsBatteryPageElement.is, SettingsBatteryPageElement);
