// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_parental_controls_subpage.html.js';

export class SettingsAppParentalControlsSubpageElement extends PolymerElement {
  static get is() {
    return 'settings-app-parental-controls-subpage';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(
    SettingsAppParentalControlsSubpageElement.is,
    SettingsAppParentalControlsSubpageElement);
