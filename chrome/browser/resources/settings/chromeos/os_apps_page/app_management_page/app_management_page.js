// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Load api_listener after other assets have initialized.

import './api_listener.js';
import './main_view.js';
import '../../../settings_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsAppManagementPageElement extends PolymerElement {
  static get is() {
    return 'settings-app-management-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {string}
       */
      searchTerm: String,
    };
  }
}

customElements.define(
    SettingsAppManagementPageElement.is, SettingsAppManagementPageElement);
