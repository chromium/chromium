// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/controls/controlled_radio_button.js';
import '/shared/settings/controls/settings_dropdown_menu.js';
import '/shared/settings/controls/settings_radio_group.js';
import '/shared/settings/controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '../settings_shared.css.js';
import './tab_discard_exception_list.js';

import {DropdownMenuOptionList} from '/shared/settings/controls/settings_dropdown_menu.js';
import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getDiscardTimerOptions} from './discard_timer_options.js';
import {HighEfficiencyModeState, PerformanceMetricsProxy, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getTemplate} from './performance_page.html.js';
import {TabDiscardExceptionListElement} from './tab_discard_exception_list.js';

export const HIGH_EFFICIENCY_MODE_PREF =
    'performance_tuning.high_efficiency_mode.state';

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

      isHighEfficiencyMultistateModeEnabled_: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isHighEfficiencyMultistateModeEnabled');
        },
      },

      showHighEfficiencyHeuristicModeRecommendedBadge_: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('highEfficiencyShowRecommendedBadge');
        },
      },

      highEfficiencyModeStateEnum_: {
        readOnly: true,
        type: Object,
        value: HighEfficiencyModeState,
      },
    };
  }

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private discardTimerOptions_: DropdownMenuOptionList;
  private isHighEfficiencyMultistateModeEnabled_: boolean;
  private showHighEfficiencyHeuristicModeRecommendedBadge_: boolean;

  private onChange_() {
    this.metricsProxy_.recordHighEfficiencyModeChanged(
        this.getPref<number>(HIGH_EFFICIENCY_MODE_PREF).value);
  }

  private toggleButtonCheckedValue_() {
    return this.isHighEfficiencyMultistateModeEnabled_ ?
        HighEfficiencyModeState.ENABLED :
        HighEfficiencyModeState.ENABLED_ON_TIMER;
  }

  private isHighEfficiencyModeEnabled_(value: number): boolean {
    return value !== HighEfficiencyModeState.DISABLED;
  }

  private isHighEfficiencyModeEnabledOnTimer_(value: number): boolean {
    return value === HighEfficiencyModeState.ENABLED_ON_TIMER;
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
