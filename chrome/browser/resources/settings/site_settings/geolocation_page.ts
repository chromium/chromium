// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './category_setting_exceptions.js';
import './settings_category_default_radio_group.js';
import '../controls/collapse_radio_button.js';
import '../controls/settings_radio_group.js';
import '../icons.html.js';
import '../privacy_icons.html.js';
import '../settings_page/settings_subpage.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {ContentSetting} from './constants.js';
import {getHtml} from './geolocation_page.html.js';
import {getCss as getSiteSettingsSharedCss} from './site_settings_shared_lit.css.js';

const GeolocationPageElementBase = SettingsViewMixinLit(CrLitElement);

export class GeolocationPageElement extends GeolocationPageElementBase {
  static get is() {
    return 'settings-geolocation-page';
  }

  static override get styles() {
    return [
      getSettingsSharedCss(),
      getSiteSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      searchTerm_: {type: String},

      /**
       * Glue property to connect the category default setting value to the
       * visibility of the additional CPSS options.
       */
      locationSettingValue_: {type: String},
    };
  }

  protected accessor searchTerm_: string = '';
  private accessor locationSettingValue_: string = '';

  protected onSearchTermChanged_(e: CustomEvent<{value: string}>) {
    this.searchTerm_ = e.detail.value;
  }

  protected onSelectedValueChanged_(e: CustomEvent<{value: string}>) {
    this.locationSettingValue_ = e.detail.value;
  }

  protected shouldShowCpssRadioGroup_(): boolean {
    return this.locationSettingValue_ === ContentSetting.ASK;
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-geolocation-page': GeolocationPageElement;
  }
}

customElements.define(GeolocationPageElement.is, GeolocationPageElement);
