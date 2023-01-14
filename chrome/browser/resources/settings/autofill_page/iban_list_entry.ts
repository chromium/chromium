// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'iban-list-entry' is an IBAN row to be shown on the settings
 * page.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../i18n_setup.js';
import '../settings_shared.css.js';
import './passwords_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './iban_list_entry.html.js';

export class SettingsIbanListEntryElement extends PolymerElement {
  static get is() {
    return 'settings-iban-list-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** A saved IBAN. */
      iban: Object,
    };
  }

  iban: chrome.autofillPrivate.IbanEntry;
}

customElements.define(
    SettingsIbanListEntryElement.is, SettingsIbanListEntryElement);
