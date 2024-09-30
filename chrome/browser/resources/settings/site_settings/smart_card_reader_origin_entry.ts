// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-entry' is an element representing a single origin's
 * permission grant(s), granted via the File System Access API.
 */
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './file_system_site_entry_item.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SiteSettingsMixin} from './site_settings_mixin.js';
import {getTemplate} from './smart_card_reader_origin_entry.html.js';

const SmartCardReaderOriginEntryElementBase = SiteSettingsMixin(PolymerElement);

export class SmartCardReaderOriginEntryElement extends
    SmartCardReaderOriginEntryElementBase {
  static get is() {
    return 'smart-card-reader-origin-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      smartCardReaderName: {
        type: String,
      },
      origin: {
        type: String,
      },
    };
  }
  smartCardReaderName: string;
  origin: string;

  private onRemoveOriginClick_() {
    this.browserProxy.revokeSmartCardReaderGrant(
        this.smartCardReaderName, this.origin);
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'smart-card-reader-origin-entry': SmartCardReaderOriginEntryElement;
  }
}

customElements.define(
    SmartCardReaderOriginEntryElement.is, SmartCardReaderOriginEntryElement);
