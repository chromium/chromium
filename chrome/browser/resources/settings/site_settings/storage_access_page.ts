// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';
import '../settings_shared.css.js';
import './storage_access_site_list.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContentSetting, ContentSettingsTypes} from '../site_settings/constants.js';

import {getTemplate} from './storage_access_page.html.js';

export class StorageAccessPageElement extends PolymerElement {
  static get is() {
    return 'settings-storage-access-page';
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

      // Expose ContentSetting enum to the HTML template.
      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },
    };
  }

  declare searchTerm: string;
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-storage-access-page': StorageAccessPageElement;
  }
}

customElements.define(StorageAccessPageElement.is, StorageAccessPageElement);
