// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_section.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ControlledRadioButtonElement} from '../controls/controlled_radio_button.js';
import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './battery_page.css.js';
import {getHtml} from './battery_page.html.js';
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

const SettingsBatteryPageElementBase =
    PrefServiceObserverMixinLit(CrLitElement);

export class SettingsBatteryPageElement extends SettingsBatteryPageElementBase {
  static get is() {
    return 'settings-battery-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isBatterySaverModeManagedByOs_: {type: Boolean},
      numericUncheckedValues_: {type: Array},
      batterySaverStatePref_: {type: Object},
    };
  }

  protected accessor isBatterySaverModeManagedByOs_: boolean =
      loadTimeData.getBoolean('isBatterySaverModeManagedByOS');
  protected accessor numericUncheckedValues_: BatterySaverModeState[] =
      [BatterySaverModeState.DISABLED];
  protected accessor batterySaverStatePref_: chrome.settingsPrivate.PrefObject|
      undefined;

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(BATTERY_SAVER_MODE_PREF, 'batterySaverStatePref_');
  }

  protected isBatterySaverModeEnabled_(): boolean {
    if (!this.batterySaverStatePref_) {
      return false;
    }
    return this.batterySaverStatePref_.value !== BatterySaverModeState.DISABLED;
  }

  protected onChange_() {
    this.metricsProxy_.recordBatterySaverModeChanged(
        PrefService.getInstance()
            .getPref<number>(BATTERY_SAVER_MODE_PREF)
            .value);
  }

  protected onBatterySaverSubLabelLinkClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('batterySaverLearnMoreUrl'));
  }

  // <if expr="is_chromeos">
  protected onOsPowerSettingsClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osPowerSettingsUrl'));
  }
  // </if>

  // <if expr="_google_chrome">
  protected onSendFeedback_(e: Event) {
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

export type BatteryPageElement = SettingsBatteryPageElement;

customElements.define(
    SettingsBatteryPageElement.is, SettingsBatteryPageElement);
