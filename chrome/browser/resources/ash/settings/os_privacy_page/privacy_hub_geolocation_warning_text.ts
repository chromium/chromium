// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_hub_geolocation_warning_text.html.js';

class PrivacyHubGeolocationWarningText extends PolymerElement {
  static get is() {
    return 'settings-privacy-hub-geolocation-warning-text' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      warningTextWithAnchor: {
        type: String,
        reflectToAttribute: true,
      },
    };
  }

  warningTextWithAnchor: string;

  private launchGeolocationDialog_(e: CustomEvent<{event: Event}>): void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#".
    e.detail.event.preventDefault();
    e.stopPropagation();

    this.dispatchEvent(new CustomEvent('link-clicked', {bubbles: false}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PrivacyHubGeolocationWarningText.is]: PrivacyHubGeolocationWarningText;
  }
}

customElements.define(
    PrivacyHubGeolocationWarningText.is, PrivacyHubGeolocationWarningText);
