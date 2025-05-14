// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './security_page_v2.html.js';

const SettingsSecurityPageV2ElementBase = PrefsMixin(PolymerElement);

export class SettingsSecurityPageV2Element extends
    SettingsSecurityPageV2ElementBase {
  static get is() {
    return 'settings-security-page-v2';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-page-v2': SettingsSecurityPageV2Element;
  }
}

customElements.define(
    SettingsSecurityPageV2Element.is, SettingsSecurityPageV2Element);
