// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-get-most-chrome-page' is the settings page information about how
 * to get the most out of Chrome.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './get_most_chrome_page.html.js';

export class SettingsGetMostChromePageElement extends PolymerElement {
  static get is() {
    return 'settings-get-most-chrome-page';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-get-most-chrome-page': SettingsGetMostChromePageElement;
  }
}

customElements.define(
    SettingsGetMostChromePageElement.is, SettingsGetMostChromePageElement);
