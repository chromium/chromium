// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';
import './tab_discard_exception_list.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';

import {PerformanceBrowserProxy, PerformanceBrowserProxyImpl} from './performance_browser_proxy.js';
import {PerformanceMetricsProxy, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getTemplate} from './performance_page.html.js';
import {TabDiscardExceptionListElement} from './tab_discard_exception_list.js';

export const HIGH_EFFICIENCY_MODE_PREF =
    'performance_tuning.high_efficiency_mode.enabled';

const SettingsPerformancePageElementBase = PrefsMixin(PolymerElement);

export interface SettingsPerformancePageElement {
  $: {
    tabDiscardExceptionsList: TabDiscardExceptionListElement,
    toggleButton: SettingsToggleButtonElement,
  };
}

export class SettingsPerformancePageElement extends
    SettingsPerformancePageElementBase {
  static get is() {
    return 'settings-performance-page';
  }

  static get template() {
    return getTemplate();
  }

  private browserProxy_: PerformanceBrowserProxy =
      PerformanceBrowserProxyImpl.getInstance();
  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private onLearnMoreOrSendFeedbackClick_(e: CustomEvent<string>) {
    switch (e.detail) {
      case 'highEfficiencyLearnMore':
        OpenWindowProxyImpl.getInstance().openUrl(
            loadTimeData.getString('highEfficiencyLearnMoreUrl'));
        break;
      case 'highEfficiencySendFeedback':
        this.browserProxy_.openHighEfficiencyFeedbackDialog();
        break;
    }
  }

  private onChange_() {
    this.metricsProxy_.recordHighEfficiencyModeChanged(
        this.getPref(HIGH_EFFICIENCY_MODE_PREF).value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-performance-page': SettingsPerformancePageElement;
  }
}

customElements.define(
    SettingsPerformancePageElement.is, SettingsPerformancePageElement);
