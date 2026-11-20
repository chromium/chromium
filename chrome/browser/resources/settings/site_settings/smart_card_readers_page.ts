// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './chooser_exception_list.js';
import './settings_category_default_radio_group.js';
import '../settings_page/settings_subpage.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getCss as getSiteSettingsSharedCss} from './site_settings_shared_lit.css.js';
import {getHtml} from './smart_card_readers_page.html.js';

const SmartCardReadersPageElementBase = SettingsViewMixinLit(CrLitElement);

export class SmartCardReadersPageElement extends
    SmartCardReadersPageElementBase {
  static get is() {
    return 'settings-smart-card-readers-page';
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

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-smart-card-readers-page': SmartCardReadersPageElement;
  }
}

customElements.define(
    SmartCardReadersPageElement.is, SmartCardReadersPageElement);
