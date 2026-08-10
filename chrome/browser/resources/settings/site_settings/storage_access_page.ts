// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';
import './storage_access_site_list.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';
import {ContentSetting, ContentSettingsTypes} from '../site_settings/constants.js';

import {getTemplate} from './storage_access_page.html.js';

const StorageAccessPageElementBase = SettingsViewMixin(PolymerElement);

export class StorageAccessPageElement extends StorageAccessPageElementBase {
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

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-storage-access-page': StorageAccessPageElement;
  }
}

customElements.define(StorageAccessPageElement.is, StorageAccessPageElement);
