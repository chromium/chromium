// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './chooser_exception_list.js';
import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';
import {ChooserType, ContentSettingsTypes} from '../site_settings/constants.js';

import {getTemplate} from './usb_devices_page.html.js';

const UsbDevicesPageElementBase = SettingsViewMixin(PolymerElement);

export class UsbDevicesPageElement extends UsbDevicesPageElementBase {
  static get is() {
    return 'settings-usb-devices-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Expose ContentSettingsTypes enum to the HTML template.
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },

      // Expose ChooserType enum to the HTML template.
      chooserTypeEnum_: {
        type: Object,
        value: ChooserType,
      },
    };
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-usb-devices-page': UsbDevicesPageElement;
  }
}

customElements.define(UsbDevicesPageElement.is, UsbDevicesPageElement);
