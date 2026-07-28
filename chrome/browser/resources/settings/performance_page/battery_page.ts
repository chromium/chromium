// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ControlledRadioButtonElement} from '../controls/controlled_radio_button.js';
import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './battery_page.html.js';
import {PerformanceBrowserProxyImpl, PerformanceFeedbackCategory} from './performance_browser_proxy.js';
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

const SettingsBatteryPageElementBase = PrefServiceObserverMixin(PolymerElement);

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

      isBatterySaverModeManagedByOs_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isBatterySaverModeManagedByOS');
        },
      },

      numericUncheckedValues_: {
        type: Array,
        value: () => [BatterySaverModeState.DISABLED],
      },

      batterySaverStatePref_: {type: Object},
    };
  }

  declare private isBatterySaverModeManagedByOs_: boolean;
  declare private numericUncheckedValues_: BatterySaverModeState[];
  declare private batterySaverStatePref_: chrome.settingsPrivate.PrefObject|
      undefined;

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(BATTERY_SAVER_MODE_PREF, 'batterySaverStatePref_');
  }

  private isBatterySaverModeEnabled_(): boolean {
    assert(this.batterySaverStatePref_);
    return this.batterySaverStatePref_.value !== BatterySaverModeState.DISABLED;
  }

  private onChange_() {
    this.metricsProxy_.recordBatterySaverModeChanged(
        PrefService.getInstance()
            .getPref<number>(BATTERY_SAVER_MODE_PREF)
            .value);
  }

  private onBatterySaverLearnMoreLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('batterySaverLearnMoreUrl'));
  }

  // <if expr="is_chromeos">
  private openOsPowerSettings_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osPowerSettingsUrl'));
  }
  // </if>

  // <if expr="_google_chrome">
  private onSendFeedbackClick_(e: Event) {
    e.stopPropagation();
    PerformanceBrowserProxyImpl.getInstance().openFeedbackDialog(
        PerformanceFeedbackCategory.BATTERY);
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
