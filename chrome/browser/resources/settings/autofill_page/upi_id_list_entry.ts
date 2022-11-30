// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'upi-id-list-entry' is a UPI ID row to be shown in
 * the settings page. https://en.wikipedia.org/wiki/Unified_Payments_Interface
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../i18n_setup.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './upi_id_list_entry.html.js';

class SettingsUpiIdListEntryElement extends PolymerElement {
  static get is() {
    return 'settings-upi-id-list-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** A saved UPI ID. */
      upiId: String,
    };
  }
}

customElements.define(
    SettingsUpiIdListEntryElement.is, SettingsUpiIdListEntryElement);
