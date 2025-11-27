// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './category_setting_exceptions.js';
import './site_settings_shared.css.js';
import '../controls/collapse_radio_button.js';
import '../controls/settings_radio_group.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {SafeBrowsingSetting} from '../privacy_page/safe_browsing_types.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {ContentSettingsTypes, JavascriptOptimizerSetting} from './constants.js';
import {getTemplate} from './v8_page.html.js';

const V8PageElementBase = SettingsViewMixin(I18nMixin(PrefsMixin(PolymerElement)));

export class V8PageElement extends V8PageElementBase {
  static get is() {
    return 'settings-v8-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchTerm: String,

      enableBlockV8OptimizerOnUnfamiliarSites_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableBlockV8OptimizerOnUnfamiliarSites');
        },
      },

      // Expose ContentSettingsTypes enum to the HTML template.
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },

      // Expose JavascriptOptimizerSetting enum to the HTML template.
      javascriptOptimizerSettingEnum_: {
        type: Object,
        value: JavascriptOptimizerSetting,
      },
    };
  }

  declare searchTerm: string;
  declare private enableBlockV8OptimizerOnUnfamiliarSites_: boolean;

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }

  private getBlockForUnfamiliarSitesSubLabel_(): string {
    const safeBrowsingSetting = this.getPref('generated.safe_browsing').value;
    return this.i18n(
        safeBrowsingSetting === SafeBrowsingSetting.DISABLED
            ? 'siteSettingsJavascriptOptimizerBlockedUnfamiliarSitesSafeBrowsingOffSubLabel'
            : 'siteSettingsJavascriptOptimizerBlockedUnfamiliarSitesSubLabel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-v8-page': V8PageElement;
  }
}

customElements.define(V8PageElement.is, V8PageElement);
