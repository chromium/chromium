// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './category_setting_exceptions.js';
import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContentSettingsTypes} from '../site_settings/constants.js';

import {getTemplate} from './federated_identity_api_page.html.js';

export class FederatedIdentityApiPageElement extends PolymerElement {
  static get is() {
    return 'settings-federated-identity-api-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchTerm: String,

      // Expose ContentSettingsTypes enum to the HTML template.
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },
    };
  }

  declare searchTerm: string;
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-federated-identity-api-page': FederatedIdentityApiPageElement;
  }
}

customElements.define(
    FederatedIdentityApiPageElement.is, FederatedIdentityApiPageElement);
