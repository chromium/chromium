// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-metrics-consent-toggle-button' is a toggle that controls user
 * consent regarding user metric analysis.
 */

import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {MetricsConsentBrowserProxy, MetricsConsentBrowserProxyImpl} from './metrics_consent_browser_proxy.js';
import {getTemplate} from './metrics_consent_toggle_button.html.js';

const SettingsMetricsConsentToggleButtonElementBase =
    PrefsMixin(PolymerElement);

class SettingsMetricsConsentToggleButtonElement extends
    SettingsMetricsConsentToggleButtonElementBase {
  static get is() {
    return 'settings-metrics-consent-toggle-button' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The preference controlling the current user's metrics consent. This
       * will be loaded from |this.prefs| based on the response from
       * |this.metricsConsentBrowserProxy_.getMetricsConsentState()|.
       */
      metricsConsentPref_: {
        type: Object,
        value: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },

      isMetricsConsentConfigurable_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isMetricsConsentConfigurable_: boolean;
  private metricsConsentBrowserProxy_: MetricsConsentBrowserProxy;
  private metricsConsentPref_: chrome.settingsPrivate.PrefObject<boolean>;

  constructor() {
    super();

    this.metricsConsentBrowserProxy_ =
        MetricsConsentBrowserProxyImpl.getInstance();
    this.metricsConsentBrowserProxy_.getMetricsConsentState().then(state => {
      const pref = this.get(state.prefName, this.prefs);
      if (pref) {
        this.metricsConsentPref_ = pref;
        this.isMetricsConsentConfigurable_ = state.isConfigurable;
      }
    });
  }

  override focus(): void {
    this.metricsToggle_.focus();
  }

  private async onMetricsConsentChange_(): Promise<void> {
    const consent = await this.metricsConsentBrowserProxy_.updateMetricsConsent(
        this.metricsToggle_.checked);
    if (consent === this.metricsToggle_.checked) {
      this.metricsToggle_.sendPrefChange();
    } else {
      this.metricsToggle_.resetToPrefValue();
    }
  }

  private get metricsToggle_(): SettingsToggleButtonElement {
    return castExists(
        this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#settingsToggle'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMetricsConsentToggleButtonElement.is]:
        SettingsMetricsConsentToggleButtonElement;
  }
}

customElements.define(
    SettingsMetricsConsentToggleButtonElement.is,
    SettingsMetricsConsentToggleButtonElement);
