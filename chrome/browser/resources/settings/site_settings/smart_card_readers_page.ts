// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_category_default_radio_group.js';
import './chooser_exception_list.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ChooserType, ContentSettingsTypes} from './constants.js';
import {getTemplate} from './smart_card_readers_page.html.js';

const SettingsSmartCardReadersPageElementBase = PolymerElement;

export class SettingsSmartCardReadersPageElement extends
    SettingsSmartCardReadersPageElementBase {
  static get is() {
    return 'settings-smart-card-readers-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      contentSettingsTypeEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },
      chooserTypeEnum_: {
        type: Object,
        value: ChooserType,
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-smart-card-readers-page': SettingsSmartCardReadersPageElement;
  }
}

customElements.define(
    SettingsSmartCardReadersPageElement.is,
    SettingsSmartCardReadersPageElement);
