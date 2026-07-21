// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_category_default_radio_group.js';
import './chooser_exception_list.js';
import './site_settings_shared.css.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {ChooserType, ContentSettingsTypes} from './constants.js';
import {getTemplate} from './smart_card_readers_page.html.js';

const SettingsSmartCardReadersPageElementBase =
    SettingsViewMixin(PolymerElement);

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

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }

  protected getSmartCardReaderIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'privacy:smart-card-reader' :
        'privacy:smart-card-reader-old';
  }

  protected getSmartCardReaderOffIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'privacy:smart-card-reader-off' :
        'privacy:smart-card-reader-off-old';
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
