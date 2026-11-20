// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './chooser_exception_list.js';
import './settings_category_default_radio_group.js';
import '../settings_page/settings_subpage.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getCss as getSiteSettingsSharedCss} from './site_settings_shared_lit.css.js';
import {getHtml} from './usb_devices_page.html.js';

const UsbDevicesPageElementBase = SettingsViewMixinLit(CrLitElement);

export class UsbDevicesPageElement extends UsbDevicesPageElementBase {
  static get is() {
    return 'settings-usb-devices-page';
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
    'settings-usb-devices-page': UsbDevicesPageElement;
  }
}

customElements.define(UsbDevicesPageElement.is, UsbDevicesPageElement);
