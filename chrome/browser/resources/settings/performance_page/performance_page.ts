// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';
import './tab_discard_exception_list.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

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

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private onChange_() {
    this.metricsProxy_.recordHighEfficiencyModeChanged(
        this.getPref<boolean>(HIGH_EFFICIENCY_MODE_PREF).value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-performance-page': SettingsPerformancePageElement;
  }
}

customElements.define(
    SettingsPerformancePageElement.is, SettingsPerformancePageElement);
