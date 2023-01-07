// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * `installed-app-checkbox` is a checkbox that displays an installed app.
 * An installed app could be a domain with data that the user might want
 * to protect from being deleted.
 */
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InstalledApp} from './clear_browsing_data_browser_proxy.js';
import {getTemplate} from './installed_app_checkbox.html.js';

class InstalledAppCheckboxElement extends PolymerElement {
  static get is() {
    return 'installed-app-checkbox';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      installedApp: Object,
      disabled: {
        type: Boolean,
        value: false,
      },
    };
  }

  installedApp: InstalledApp;
}

declare global {
  interface HTMLElementTagNameMap {
    'installed-app-checkbox': InstalledAppCheckboxElement;
  }
}

customElements.define(
    InstalledAppCheckboxElement.is, InstalledAppCheckboxElement);
