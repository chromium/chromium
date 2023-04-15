// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-get-most-chrome-page' is the settings page information about how
 * to get the most out of Chrome.
 */

import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './get_most_chrome_page.html.js';

export class SettingsGetMostChromePageElement extends PolymerElement {
  static get is() {
    return 'settings-get-most-chrome-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expandedMoreThanABrowser_: Boolean,
      expandedYourDataInChrome_: Boolean,
      expandedBeyondCookies_: Boolean,
    };
  }

  private expandedMoreThanABrowser_: boolean;
  private expandedYourDataInChrome_: boolean;
  private expandedBeyondCookies_: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-get-most-chrome-page': SettingsGetMostChromePageElement;
  }
}

customElements.define(
    SettingsGetMostChromePageElement.is, SettingsGetMostChromePageElement);
