// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContentSettingsTypes} from './constants.js';
import {getTemplate} from './smart_card_readers_page.html.js';

export class SettingsSmartCardReadersPageElement extends PolymerElement {
  static get is() {
    return 'settings-smart-card-readers-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      contentSettingsType_: {
        type: ContentSettingsTypes,
        value: ContentSettingsTypes.SMART_CARD_READERS,
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
