// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './office_page.html.js';

const SettingsOfficePageElementBase = PrefsMixin(PolymerElement);

export class SettingsOfficePageElement extends SettingsOfficePageElementBase {
  static get is() {
    return 'settings-office-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  // TODO(b:264314789): Do we need focus for deep linking?
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsOfficePageElement.is]: SettingsOfficePageElement;
  }
}

customElements.define(SettingsOfficePageElement.is, SettingsOfficePageElement);
