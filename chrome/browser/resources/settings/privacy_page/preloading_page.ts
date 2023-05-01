// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './collapse_radio_button.js';
import '/shared/settings/controls/settings_radio_group.js';
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import '../settings_shared.css.js';
import './privacy_guide/privacy_guide_fragment_shared.css.js';

import {SettingsRadioGroupElement} from '/shared/settings/controls/settings_radio_group.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrSettingsPrefs} from 'chrome://resources/cr_components/settings_prefs/prefs_types.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsCollapseRadioButtonElement} from './collapse_radio_button.js';
import {NetworkPredictionOptions} from './cookies_page.js';
import {getTemplate} from './preloading_page.html.js';

export interface PreloadingPageElement {
  $: {
    preloadingDisabled: SettingsCollapseRadioButtonElement,
    preloadingExtended: SettingsCollapseRadioButtonElement,
    preloadingRadioGroup: SettingsRadioGroupElement,
    preloadingStandard: SettingsCollapseRadioButtonElement,
  };
}

const PreloadingPageElementBase = PrefsMixin(PolymerElement);

export class PreloadingPageElement extends PreloadingPageElementBase {
  static get is() {
    return 'settings-preloading-page';
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
    };
  }

  override ready() {
    super.ready();

    CrSettingsPrefs.initialized.then(() => {
      // Expand initial pref value manually because automatic
      // expanding is disabled.
      const prefValue = this.getPref<NetworkPredictionOptions>(
                                'net.network_prediction_options')
                            .value;
      switch (prefValue) {
        case NetworkPredictionOptions.EXTENDED:
          this.$.preloadingExtended.expanded = true;
          return;
        case NetworkPredictionOptions.STANDARD:
          this.$.preloadingStandard.expanded = true;
          return;
        case NetworkPredictionOptions.WIFI_ONLY_DEPRECATED:
          // The default pref value is deprecated, and is treated the same as
          // STANDARD. See chrome/browser/prefetch/prefetch_prefs.h.
          this.setPrefValue(
              'net.network_prediction_options',
              NetworkPredictionOptions.STANDARD);
          this.$.preloadingStandard.expanded = true;
          return;
        case NetworkPredictionOptions.DISABLED:
          return;
        default:
          assertNotReached();
      }
    });
  }

  private onPreloadingRadioChange_() {
    this.$.preloadingExtended.updateCollapsed();
    this.$.preloadingStandard.updateCollapsed();
    this.$.preloadingRadioGroup.sendPrefChange();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-preloading-page': PreloadingPageElement;
  }
}

customElements.define(PreloadingPageElement.is, PreloadingPageElement);
