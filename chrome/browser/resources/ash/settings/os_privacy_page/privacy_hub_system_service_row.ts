// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-system-service-row' is a custom row element
 * representing a system service or system app. This is used in the subpages of
 * the OS Settings Privacy controls page.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_hub_system_service_row.html.js';

export class SettingsPrivacyHubSystemServiceRow extends PolymerElement {
  static get is() {
    return 'settings-privacy-hub-system-service-row' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      name: {
        type: String,
        value: '',
      },

      permissionState: {
        type: String,
        value: '',
      },

      iconSource: {
        type: String,
        value: '',
      },
    };
  }

  iconSource: string;
  name: string;
  permissionStatus: string;
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubSystemServiceRow.is]: SettingsPrivacyHubSystemServiceRow;
  }
}

customElements.define(
    SettingsPrivacyHubSystemServiceRow.is, SettingsPrivacyHubSystemServiceRow);
