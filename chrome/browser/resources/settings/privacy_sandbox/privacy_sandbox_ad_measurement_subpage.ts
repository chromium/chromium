// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '/shared/settings/prefs/prefs.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_columned_section.css.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './privacy_sandbox_ad_measurement_subpage.html.js';

export interface SettingsPrivacySandboxAdMeasurementSubpageElement {
  $: {
    adMeasurementToggle: SettingsToggleButtonElement,
  };
}

const SettingsPrivacySandboxAdMeasurementSubpageElementBase =
    SettingsViewMixin(PrefsMixin(PolymerElement));

export class SettingsPrivacySandboxAdMeasurementSubpageElement extends
    SettingsPrivacySandboxAdMeasurementSubpageElementBase {
  static get is() {
    return 'settings-privacy-sandbox-ad-measurement-subpage';
  }

  static get template() {
    return getTemplate();
  }

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

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
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
