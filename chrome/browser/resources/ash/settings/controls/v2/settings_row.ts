// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseRowMixin} from './base_row_mixin.js';
import {getTemplate} from './settings_row.html.js';

const SettingsRowElementBase = BaseRowMixin(PolymerElement);

export class SettingsRowElement extends SettingsRowElementBase {
  static get is() {
    return 'settings-row' as const;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsRowElement.is]: SettingsRowElement;
  }
}

customElements.define(SettingsRowElement.is, SettingsRowElement);
