// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/collapse_radio_button.js';
import '../controls/settings_dropdown_menu.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import '../settings_columned_section.css.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsCollapseRadioButtonElement} from '../controls/collapse_radio_button.js';
import type {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import {NetworkPredictionOptions} from './constants.js';
import type {CpuPerformanceInfo} from './performance_browser_proxy.js';
import {PerformanceBrowserProxyImpl, PerformanceFeedbackCategory} from './performance_browser_proxy.js';
import {getTemplate} from './speed_page.html.js';

export interface SpeedPageElement {
  $: {
    preloadingToggle: SettingsToggleButtonElement,
    preloadingExtended: SettingsCollapseRadioButtonElement,
    preloadingRadioGroup: SettingsRadioGroupElement,
    preloadingStandard: SettingsCollapseRadioButtonElement,
  };
}

const SpeedPageElementBase = PrefServiceObserverMixin(PolymerElement);
export const PRELOADING_PREF = 'net.network_prediction_options';

export class SpeedPageElement extends SpeedPageElementBase {
  static get is() {
    return 'settings-speed-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Valid network prediction options state. */
      networkPredictionOptionsEnum_: {
        type: Object,
        value: NetworkPredictionOptions,
      },

      numericUncheckedValues_: {
        type: Array,
        value: () => [NetworkPredictionOptions.DISABLED],
      },

      cpuPerformanceInfo_: {
        type: Object,
        value: null,
      },

      cpuPerformanceModelLabel_: {
        type: String,
        value: '',
      },

      cpuPerformanceEnabled_: {
        type: Boolean,
        readOnly: true,
        value: () => loadTimeData.getBoolean('cpuPerformanceEnabled'),
      },

      cpuPerformanceTierOptions_: {
        type: Array,
        readOnly: true,
        value: () => ([
          {
            value: -1,
            name: loadTimeData.getString('cpuPerformanceTierDefault'),
          },
          {
            value: 0,
            name: loadTimeData.getString('cpuPerformanceTierUnknown'),
          },
          {
            value: 1,
            name: loadTimeData.getString('cpuPerformanceTierLow'),
          },
          {
            value: 2,
            name: loadTimeData.getString('cpuPerformanceTierMid'),
          },
          {
            value: 3,
            name: loadTimeData.getString('cpuPerformanceTierHigh'),
          },
          {
            value: 4,
            name: loadTimeData.getString('cpuPerformanceTierUltra'),
          },
        ]),
      },

      preloadingStatePref_: {type: Object},
    };
  }

  declare private cpuPerformanceInfo_: CpuPerformanceInfo|null;
  declare private cpuPerformanceModelLabel_: string;
  declare private cpuPerformanceEnabled_: boolean;
  declare private cpuPerformanceTierOptions_: DropdownMenuOptionList;
  declare private numericUncheckedValues_: NetworkPredictionOptions[];
  declare private preloadingStatePref_: chrome.settingsPrivate.PrefObject|
      undefined;

  override ready() {
    super.ready();

    PrefService.getInstance().whenInitialized().then(() => {
      const prefValue = PrefService.getInstance()
                            .getPref<NetworkPredictionOptions>(PRELOADING_PREF)
                            .value;
      if (prefValue === NetworkPredictionOptions.WIFI_ONLY_DEPRECATED) {
        // The default pref value is deprecated, and is treated the same as
        // STANDARD. See chrome/browser/preloading/preloading_prefs.h.
        PrefService.getInstance().setPrefValue(
            PRELOADING_PREF, NetworkPredictionOptions.STANDARD);
      }
    });

    PerformanceBrowserProxyImpl.getInstance().getCpuPerformanceInfo().then(
        async info => {
          this.cpuPerformanceInfo_ = info;
          const coresText =
              await PluralStringProxyImpl.getInstance().getPluralString(
                  'cpuPerformanceCores', info.cores);
          this.cpuPerformanceModelLabel_ = loadTimeData.getStringF(
              'cpuPerformanceModel', info.model, coresText);
        });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(PRELOADING_PREF, 'preloadingStatePref_');
  }

  private isPreloadingEnabled_(): boolean {
    assert(this.preloadingStatePref_);
    return this.preloadingStatePref_.value !==
        NetworkPredictionOptions.DISABLED;
  }

  private onPreloadingStateChange_() {
    // Automatic expanding is disabled so that the radio buttons are collapsed
    // initially. Because of this, radio buttons' expanded states need to be
    // updated manually.
    // Defer updates to let the pref change propagate to the radio buttons
    // first.
    Promise.resolve().then(() => {
      this.$.preloadingExtended.updateCollapsed();
      this.$.preloadingStandard.updateCollapsed();
    });
  }

  private onPreloadingLearnMoreLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('preloadingLearnMoreUrl'));
  }

  // <if expr="_google_chrome">
  private onSendFeedbackClick_(e: Event) {
    e.stopPropagation();
    PerformanceBrowserProxyImpl.getInstance().openFeedbackDialog(
        PerformanceFeedbackCategory.SPEED);
  }
  // </if>

  private getCpuPerformanceNominalTierLabel_(): string {
    const tierLabels = [
      loadTimeData.getString('cpuPerformanceTierUnknown'),
      loadTimeData.getString('cpuPerformanceTierLow'),
      loadTimeData.getString('cpuPerformanceTierMid'),
      loadTimeData.getString('cpuPerformanceTierHigh'),
      loadTimeData.getString('cpuPerformanceTierUltra'),
    ];
    const hardwareTier = this.cpuPerformanceInfo_?.hardwareTier ?? 0;
    const tierLabel = tierLabels[hardwareTier];
    assert(tierLabel, `Unexpected hardwareTier encountered ${hardwareTier}`);
    return loadTimeData.getStringF('cpuPerformanceNominalTier', tierLabel);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-speed-page': SpeedPageElement;
  }
}

customElements.define(SpeedPageElement.is, SpeedPageElement);
