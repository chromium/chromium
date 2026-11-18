// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './category_setting_exceptions.js';
import './media_picker.js';
import './settings_category_default_radio_group.js';
import '../settings_page/settings_subpage.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getHtml} from './camera_page.html.js';
import {getCss as getSiteSettingsSharedCss} from './site_settings_shared_lit.css.js';

const CameraPageElementBase = SettingsViewMixinLit(CrLitElement);

export class CameraPageElement extends CameraPageElementBase {
  static get is() {
    return 'settings-camera-page';
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
    'settings-camera-page': CameraPageElement;
  }
}

customElements.define(CameraPageElement.is, CameraPageElement);
