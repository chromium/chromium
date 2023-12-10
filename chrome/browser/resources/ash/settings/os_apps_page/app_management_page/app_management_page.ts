// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './main_view.js';
import '../../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_management_page.html.js';

export class SettingsAppManagementPageElement extends PolymerElement {
  static get is() {
    return 'settings-app-management-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchTerm: String,
    };
  }

  searchTerm: string;
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-app-management-page': SettingsAppManagementPageElement;
  }
}

customElements.define(
    SettingsAppManagementPageElement.is, SettingsAppManagementPageElement);
