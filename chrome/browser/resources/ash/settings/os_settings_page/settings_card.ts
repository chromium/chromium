// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-card' shows a paper material themed card with an optional
 * header.
 *
 * Example:
 *    <settings-card header-text="[[headerText]]">
 *      <!-- Insert card content here -->
 *    </settings-card>
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './settings_card.html.js';

export class SettingsCardElement extends PolymerElement {
  static get is() {
    return 'settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      headerText: {
        type: String,
        value: '',
      },
    };
  }

  headerText: string;
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsCardElement.is]: SettingsCardElement;
  }
}

customElements.define(SettingsCardElement.is, SettingsCardElement);
