// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-row' is a foundational component to build more complex types of
 * rows (e.g. toggle row, slider, row, etc.). This component may also be used
 * directly for simple use-cases.
 *
 * - Usage: With label and optional sublabel
 *
 *   <settings-row label="Lorem ipsum"
 *       sublabel="Lorem ipsum dolor sit amet">
 *   </settings-row>
 *
 * - With optional icon
 *   - Usage: iron-icon
 *
 *     <settings-row label="Lorem ipsum"
 *         sublabel="Lorem ipsum dolor sit amet"
 *         icon="os-settings:display">
 *     </settings-row>
 *
 *   - Usage: Slotted icon element
 *
 *     <settings-row label="Lorem ipsum"
 *         sublabel="Lorem ipsum dolor sit amet">
 *       <img src="#" slot="icon"></img>
 *     </settings-row>
 *
 * - Usage: With optional learn more link
 *
 *   <settings-row label="Lorem ipsum"
 *       sublabel="Lorem ipsum dolor sit amet"
 *       learn-more-url="https://google.com/">
 *   </settings-row>
 *
 * - Usage: With optional slotted control element
 *
 *   <settings-row label="Lorem ipsum"
 *       sublabel="Lorem ipsum dolor sit amet">
 *     <settings-toggle-v2 slot="control"></settings-toggle-v2>
 *   </settings-row>
 */

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
