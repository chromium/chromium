// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import {getTemplate} from './privacy_sandbox_ad_measurement_subpage.html.js';

export interface SettingsPrivacySandboxAdMeasurementSubpageElement {
  $: {
    adMeasurementToggle: SettingsToggleButtonElement,
  };
}

const SettingsPrivacySandboxAdMeasurementSubpageElementBase =
    PrefsMixin(PolymerElement);

export class SettingsPrivacySandboxAdMeasurementSubpageElement extends
    SettingsPrivacySandboxAdMeasurementSubpageElementBase {
  static get is() {
    return 'settings-privacy-sandbox-ad-measurement-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * If true, the Ads API UX Enhancement should be shown.
       */
      shouldShowV2_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean(
              'isPrivacySandboxAdsApiUxEnhancementsEnabled');
        },
      },
    };
  }

  declare private shouldShowV2_: boolean;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private onToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    this.metricsBrowserProxy_.recordAction(
        target.checked ? 'Settings.PrivacySandbox.AdMeasurement.Enabled' :
                         'Settings.PrivacySandbox.AdMeasurement.Disabled');
  }

  private onPrivacyPolicyLinkClicked_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.AdMeasurement.PrivacyPolicyLinkClicked');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-ad-measurement-subpage':
        SettingsPrivacySandboxAdMeasurementSubpageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxAdMeasurementSubpageElement.is,
    SettingsPrivacySandboxAdMeasurementSubpageElement);
