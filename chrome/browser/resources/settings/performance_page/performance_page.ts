// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/controlled_radio_button.js';
import '../controls/settings_dropdown_menu.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import '../settings_shared.css.js';
import './tab_discard/exception_list.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import type {PerformanceMetricsProxy} from './performance_metrics_proxy.js';
import {MemorySaverModeAggressiveness, MemorySaverModeState, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getTemplate} from './performance_page.html.js';
import type {ExceptionListElement} from './tab_discard/exception_list.js';

export const MEMORY_SAVER_MODE_PREF =
    'performance_tuning.high_efficiency_mode.state';

export const MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF =
    'performance_tuning.high_efficiency_mode.aggressiveness';

export const DISCARD_RING_PREF =
    'performance_tuning.discard_ring_treatment.enabled';

// browser_element_identifiers constants
const INACTIVE_TAB_SETTING_ELEMENT_ID = 'kInactiveTabSettingElementId';

const SettingsPerformancePageElementBase =
    HelpBubbleMixin(PrefsMixin(PolymerElement));

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
      isMemorySaverModeAggressivenessEnabled_: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isMemorySaverModeAggressivenessEnabled');
        },
      },

      isDiscardRingImprovementsEnabled_: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isDiscardRingImprovementsEnabled');
        },
      },

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

  private isMemorySaverModeAggressivenessEnabled_: boolean;

  private isDiscardRingImprovementsEnabled_: boolean;

  override ready() {
    super.ready();
    // Remove afterNextRender when feature is launched and dom-if is removed.
    afterNextRender(this, () => {
      const discardRingTreatmentToggleButton =
          this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#discardRingTreatmentToggleButton');
      if (discardRingTreatmentToggleButton) {
        this.registerHelpBubble(
            INACTIVE_TAB_SETTING_ELEMENT_ID,
            discardRingTreatmentToggleButton.getBubbleAnchor());
      }
    });
  }

  private onMemorySaverModeChange_() {
    this.metricsProxy_.recordMemorySaverModeChanged(
        this.getPref<number>(MEMORY_SAVER_MODE_PREF).value);
  }

  private onMemorySaverModeAggressivenessChange_() {
    this.metricsProxy_.recordMemorySaverModeAggressivenessChanged(
        this.getPref<number>(MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF).value);
  }

  private onDiscardRingChange_() {
    this.metricsProxy_.recordDiscardRingTreatmentEnabledChanged(
        this.getPref<boolean>(DISCARD_RING_PREF).value);
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
