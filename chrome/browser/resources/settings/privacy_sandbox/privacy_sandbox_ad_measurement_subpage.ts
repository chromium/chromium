// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';
import '../controls/settings_toggle_button.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin} from '../router.js';

import {getTemplate} from './privacy_sandbox_ad_measurement_subpage.html.js';

export interface SettingsPrivacySandboxAdMeasurementSubpageElement {
  $: {
    adMeasurementToggle: SettingsToggleButtonElement,
  };
}

const SettingsPrivacySandboxAdMeasurementSubpageElementBase =
    RouteObserverMixin(PrefsMixin(PolymerElement));

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
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override currentRouteChanged(newRoute: Route) {
    if (newRoute === routes.PRIVACY_SANDBOX_AD_MEASUREMENT) {
      HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
          TrustSafetyInteraction.OPENED_AD_MEASUREMENT_SUBPAGE);
    }
  }

  private onToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    this.metricsBrowserProxy_.recordAction(
        target.checked ? 'Settings.PrivacySandbox.AdMeasurement.Enabled' :
                         'Settings.PrivacySandbox.AdMeasurement.Disabled');
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
