// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_page.html.js';

export class SettingsPrivacySandboxPageElement extends PolymerElement {
  static get is() {
    return 'settings-privacy-sandbox-page';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-page': SettingsPrivacySandboxPageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxPageElement.is, SettingsPrivacySandboxPageElement);
