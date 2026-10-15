// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './category_setting_exceptions.js';
import './site_settings_shared.css.js';
import '../controls/collapse_radio_button.js';
import '../controls/settings_radio_group.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SafeBrowsingSetting} from '../privacy_page/security/safe_browsing_types.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {ContentSettingsTypes, JavascriptOptimizerSetting} from './constants.js';
import {getTemplate} from './v8_page.html.js';

const V8PageElementBase =
    SettingsViewMixin(I18nMixin(PrefServiceObserverMixin(PolymerElement)));

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

      blockForUnfamiliarSitesSubLabel_: {
        type: String,
        value: '',
      },
    };
  }

  declare searchTerm: string;
  declare private blockForUnfamiliarSitesSubLabel_: string;

  override connectedCallback() {
    super.connectedCallback();
    this.addPrefObserver<
        SafeBrowsingSetting>('generated.safe_browsing', pref => {
      this.blockForUnfamiliarSitesSubLabel_ = this.i18n(
          pref.value === SafeBrowsingSetting.DISABLED ?
              'siteSettingsJavascriptOptimizerBlockedUnfamiliarSitesSafeBrowsingOffSubLabel' :
              'siteSettingsJavascriptOptimizerBlockedUnfamiliarSitesSubLabel');
    });
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-v8-page': V8PageElement;
  }
}

customElements.define(V8PageElement.is, V8PageElement);
