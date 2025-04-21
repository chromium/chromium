// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';

import {getTemplate} from './incognito_tracking_protections_page.html.js';

const IncognitoTrackingProtectionsPageElementBase = PrefsMixin(PolymerElement);

export class IncognitoTrackingProtectionsPageElement extends
    IncognitoTrackingProtectionsPageElementBase {
  static get is() {
    return 'incognito-tracking-protections-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isIpProtectionAvailable_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isIpProtectionUxEnabled'),
      },

      isFingerprintingProtectionAvailable_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('isFingerprintingProtectionUxEnabled'),
      },
    };
  }

  declare private isIpProtectionAvailable_: boolean;
  declare private isFingerprintingProtectionAvailable_: boolean;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private onFpProtectionChanged_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.FINGERPRINTING_PROTECTION);
  }

  private onIpProtectionChanged_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.IP_PROTECTION);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'incognito-tracking-protections-page': IncognitoTrackingProtectionsPageElement;
  }
}

customElements.define(
    IncognitoTrackingProtectionsPageElement.is, IncognitoTrackingProtectionsPageElement);
