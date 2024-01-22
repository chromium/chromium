// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-geolocation-advanced-subpage' contains advance
 * settings regarding geolocation. For example: Google Location Accuracy.
 *
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_hub_geolocation_advanced_subpage.html.js';

export class SettingsPrivacyHubGeolocationAdvancedSubpage extends
    PolymerElement {
  static get is() {
    return 'settings-privacy-hub-geolocation-advanced-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubGeolocationAdvancedSubpage.is]:
    SettingsPrivacyHubGeolocationAdvancedSubpage;
  }
}

customElements.define(
  SettingsPrivacyHubGeolocationAdvancedSubpage.is,
  SettingsPrivacyHubGeolocationAdvancedSubpage);
