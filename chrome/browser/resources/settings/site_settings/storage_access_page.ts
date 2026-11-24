// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../settings_page/settings_subpage.js';
import './settings_category_default_radio_group.js';
import './storage_access_site_list.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getCss as getSiteSettingsSharedCss} from './site_settings_shared_lit.css.js';
import {getHtml} from './storage_access_page.html.js';

const StorageAccessPageElementBase = SettingsViewMixinLit(CrLitElement);

export class StorageAccessPageElement extends StorageAccessPageElementBase {
  static get is() {
    return 'settings-storage-access-page';
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
    'settings-storage-access-page': StorageAccessPageElement;
  }
}

customElements.define(StorageAccessPageElement.is, StorageAccessPageElement);
