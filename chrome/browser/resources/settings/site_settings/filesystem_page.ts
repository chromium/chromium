// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './category_setting_exceptions.js';
import './file_system_site_list.js';
import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';
import {ContentSettingsTypes} from '../site_settings/constants.js';

import {getTemplate} from './filesystem_page.html.js';

const FilesystemPageElementBase = SettingsViewMixin(PolymerElement);

export class FilesystemPageElement extends FilesystemPageElementBase {
  static get is() {
    return 'settings-filesystem-page';
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

      /**
       * Whether the File System Access Persistent Permissions UI should be
       * displayed.
       */
      enablePersistentPermissions_: {
        type: Boolean,
        readOnly: true,
        value: () => {
          return loadTimeData.getBoolean('enablePersistentPermissions');
        },
      },

    };
  }

  declare searchTerm: string;
  declare private enablePersistentPermissions_: boolean;

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-filesystem-page': FilesystemPageElement;
  }
}

customElements.define(FilesystemPageElement.is, FilesystemPageElement);
