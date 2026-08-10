// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import './category_setting_exceptions.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';
import {ContentSettingsTypes} from '../site_settings/constants.js';

import {getTemplate} from './local_fonts_page.html.js';

const LocalFontsPageElementBase = SettingsViewMixin(PolymerElement);

export class LocalFontsPageElement extends LocalFontsPageElementBase {
  static get is() {
    return 'settings-local-fonts-page';
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

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-local-fonts-page': LocalFontsPageElement;
  }
}

customElements.define(LocalFontsPageElement.is, LocalFontsPageElement);
