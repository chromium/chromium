// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-page' is the settings page that presents the safety
 * state of Chrome.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './safety_hub_page.html.js';

export class SettingsSafetyHubPageElement extends PolymerElement {
  static get is() {
    return 'settings-safety-hub-page';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-page': SettingsSafetyHubPageElement;
  }
}

customElements.define(
    SettingsSafetyHubPageElement.is, SettingsSafetyHubPageElement);
