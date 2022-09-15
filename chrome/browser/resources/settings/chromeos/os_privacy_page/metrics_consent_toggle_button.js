// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-metrics-consent-toggle-button' is a toggle that controls user
 * consent regarding user metric analysis.
 */

import '../../controls/settings_toggle_button.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsConsentBrowserProxy, MetricsConsentBrowserProxyImpl} from './metrics_consent_browser_proxy.js';

/** @polymer */
class SettingsMetricsConsentToggleButtonElement extends PolymerElement {
  static get is() {
    return 'settings-metrics-consent-toggle-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * The preference controlling the current user's metrics consent. This
       * will be loaded from |this.prefs| based on the response from
       * |this.metricsConsentBrowserProxy_.getMetricsConsentState()|.
       *
       * @private
       * @type {!chrome.settingsPrivate.PrefObject}
       */
      metricsConsentPref_: {
        type: Object,
        value: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },

      /** @private */
      isMetricsConsentConfigurable_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    this.metricsConsentBrowserProxy_ =
        MetricsConsentBrowserProxyImpl.getInstance();
    this.metricsConsentBrowserProxy_.getMetricsConsentState().then(state => {
      const pref = /** @type {?chrome.settingsPrivate.PrefObject} */ (
          this.get(state.prefName, this.prefs));
      if (pref) {
        this.metricsConsentPref_ = pref;
        this.isMetricsConsentConfigurable_ = state.isConfigurable;
      }
    });
  }

  /** @override */
  focus() {
    this.getMetricsToggle_().focus();
  }

  /** @private */
  onMetricsConsentChange_() {
    this.metricsConsentBrowserProxy_
        .updateMetricsConsent(this.getMetricsToggle_().checked)
        .then(consent => {
          if (consent === this.getMetricsToggle_().checked) {
            this.getMetricsToggle_().sendPrefChange();
          } else {
            this.getMetricsToggle_().resetToPrefValue();
          }
        });
  }

  /**
   * @private
   * @return {SettingsToggleButtonElement}
   */
  getMetricsToggle_() {
    return /** @type {SettingsToggleButtonElement} */ (
        this.shadowRoot.querySelector('#settingsToggle'));
  }
}

customElements.define(
    SettingsMetricsConsentToggleButtonElement.is,
    SettingsMetricsConsentToggleButtonElement);
