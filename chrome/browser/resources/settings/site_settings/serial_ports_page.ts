// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './chooser_exception_list.js';
import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ChooserType, ContentSettingsTypes} from '../site_settings/constants.js';

import {getTemplate} from './serial_ports_page.html.js';

export class SerialPortsPageElement extends PolymerElement {
  static get is() {
    return 'settings-serial-ports-page';
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
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-serial-ports-page': SerialPortsPageElement;
  }
}

customElements.define(SerialPortsPageElement.is, SerialPortsPageElement);
