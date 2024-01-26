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

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {getDiscardTimerOptions} from './discard_timer_options.js';
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
      /**
       * List of options for the discard timer drop-down menu.
       */
      discardTimerOptions_: {
        readOnly: true,
        type: Array,
        value: getDiscardTimerOptions,
      },

      isMemorySaverMultistateModeEnabled_: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isMemorySaverMultistateModeEnabled');
        },
      },

      showMemorySaverHeuristicModeRecommendedBadge_: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('memorySaverShowRecommendedBadge');
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
    };
  }

  private numericUncheckedValues_: MemorySaverModeState[];
  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private discardTimerOptions_: DropdownMenuOptionList;
  private isMemorySaverMultistateModeEnabled_: boolean;
  private showMemorySaverHeuristicModeRecommendedBadge_: boolean;

  private onChange_() {
    this.metricsProxy_.recordMemorySaverModeChanged(
        this.getPref<number>(MEMORY_SAVER_MODE_PREF).value);
  }

  private toggleButtonCheckedValue_() {
    return this.isMemorySaverMultistateModeEnabled_ ?
        MemorySaverModeState.ENABLED :
        MemorySaverModeState.ENABLED_ON_TIMER;
  }

  private isMemorySaverModeEnabled_(value: number): boolean {
    return value !== MemorySaverModeState.DISABLED;
  }

  private isMemorySaverModeEnabledOnTimer_(value: number): boolean {
    return value === MemorySaverModeState.ENABLED_ON_TIMER;
  }

  private onDropdownClick_(e: Event) {
    e.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-performance-page': SettingsPerformancePageElement;
  }
}

customElements.define(
    SettingsPerformancePageElement.is, SettingsPerformancePageElement);
