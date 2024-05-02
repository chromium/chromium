// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/controlled_radio_button.js';
import '../controls/settings_dropdown_menu.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '../settings_shared.css.js';
import './tab_discard/exception_list.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import type {PerformanceMetricsProxy} from './performance_metrics_proxy.js';
import {MemorySaverModeState, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getTemplate} from './performance_page.html.js';
import type {ExceptionListElement} from './tab_discard/exception_list.js';

export const MEMORY_SAVER_MODE_PREF =
    'performance_tuning.high_efficiency_mode.state';

const SettingsPerformancePageElementBase = PrefsMixin(PolymerElement);

export interface SettingsPerformancePageElement {
  $: {
    exceptionList: ExceptionListElement,
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

  static get properties() {
    return {
      isMemorySaverMultistateModeEnabled_: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isMemorySaverMultistateModeEnabled');
        },
      },

      memorySaverModeStateEnum_: {
        readOnly: true,
        type: Object,
        value: MemorySaverModeState,
      },

      numericUncheckedValues_: {
        type: Array,
        value: () => [MemorySaverModeState.DISABLED],
      },

      numericCheckedValue_: {
        type: Number,
        value: () => MemorySaverModeState.ENABLED,
      },
    };
  }

  private numericUncheckedValues_: MemorySaverModeState[];
  private numericCheckedValue_: MemorySaverModeState[];
  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private isMemorySaverMultistateModeEnabled_: boolean;

  private onChange_() {
    this.metricsProxy_.recordMemorySaverModeChanged(
        this.getPref<number>(MEMORY_SAVER_MODE_PREF).value);
  }

  private isMemorySaverModeEnabled_(value: number): boolean {
    return value !== MemorySaverModeState.DISABLED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-performance-page': SettingsPerformancePageElement;
  }
}

customElements.define(
    SettingsPerformancePageElement.is, SettingsPerformancePageElement);
