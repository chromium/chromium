// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../controls/v2/settings_row.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './settings_row_storybook.html.js';

export class SettingsRowStorybook extends PolymerElement {
  static get is() {
    return 'settings-row-storybook' as const;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsRowStorybook.is]: SettingsRowStorybook;
  }
}

customElements.define(SettingsRowStorybook.is, SettingsRowStorybook);
