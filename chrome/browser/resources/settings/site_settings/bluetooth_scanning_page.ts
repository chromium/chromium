// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_category_default_radio_group.js';
import '../settings_page/settings_subpage.js';
import './category_setting_exceptions.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getHtml} from './bluetooth_scanning_page.html.js';
import {getCss as getSiteSettingsSharedCss} from './site_settings_shared_lit.css.js';

const BluetoothScanningPageElementBase = SettingsViewMixinLit(CrLitElement);

export class BluetoothScanningPageElement extends
    BluetoothScanningPageElementBase {
  static get is() {
    return 'settings-bluetooth-scanning-page';
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
    };
  }

  protected accessor searchTerm_: string = '';

  protected onSearchTermChanged_(e: CustomEvent<{value: string}>) {
    this.searchTerm_ = e.detail.value;
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-bluetooth-scanning-page': BluetoothScanningPageElement;
  }
}

customElements.define(
    BluetoothScanningPageElement.is, BluetoothScanningPageElement);
