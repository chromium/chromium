// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {getTemplate} from './memory_page.html.js';
import type {PerformanceMetricsProxy} from './performance_metrics_proxy.js';
import {MemorySaverModeAggressiveness, MemorySaverModeState, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';

export const MEMORY_SAVER_MODE_PREF =
    'performance_tuning.high_efficiency_mode.state';

export const MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF =
    'performance_tuning.high_efficiency_mode.aggressiveness';

const SettingsMemoryPageElementBase = PrefsMixin(PolymerElement);

export interface SettingsMemoryPageElement {
  $: {
    toggleButton: SettingsToggleButtonElement,
  };
}

export class SettingsMemoryPageElement extends SettingsMemoryPageElementBase {
  static get is() {
    return 'settings-memory-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      memorySaverModeAggressivenessEnum_: {
        readOnly: true,
        type: Object,
        value: MemorySaverModeAggressiveness,
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

  private onMemorySaverModeChange_() {
    this.metricsProxy_.recordMemorySaverModeChanged(
        this.getPref<number>(MEMORY_SAVER_MODE_PREF).value);
  }

  private onMemorySaverModeAggressivenessChange_() {
    this.metricsProxy_.recordMemorySaverModeAggressivenessChanged(
        this.getPref<number>(MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF).value);
  }

  private isMemorySaverModeEnabled_(value: number): boolean {
    return value !== MemorySaverModeState.DISABLED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-memory-page': SettingsMemoryPageElement;
  }
}

customElements.define(SettingsMemoryPageElement.is, SettingsMemoryPageElement);
