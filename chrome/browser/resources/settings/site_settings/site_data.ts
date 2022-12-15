// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-data' is the polymer element for showing the
 * settings for site data under Site Settings.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './site_data.html.js';

class SettingsSiteDataElement extends PolymerElement {
  static get is() {
    return 'settings-site-data';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      /** Current search term. */
      searchTerm: {
        type: String,
        notify: true,
        value: '',
      },
    };
  }
}

customElements.define(SettingsSiteDataElement.is, SettingsSiteDataElement);
