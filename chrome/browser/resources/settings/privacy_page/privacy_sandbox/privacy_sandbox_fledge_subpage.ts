// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_fledge_subpage.html.js';

export class SettingsPrivacySandboxFledgeSubpageElement extends PolymerElement {
  static get is() {
    return 'settings-privacy-sandbox-fledge-subpage';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-fledge-subpage':
        SettingsPrivacySandboxFledgeSubpageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxFledgeSubpageElement.is,
    SettingsPrivacySandboxFledgeSubpageElement);
