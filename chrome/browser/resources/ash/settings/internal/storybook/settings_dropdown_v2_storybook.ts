// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../controls/v2/settings_dropdown_v2.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {DropdownOptionList} from '../../controls/v2/settings_dropdown_v2.js';

import {getTemplate} from './settings_dropdown_v2_storybook.html.js';

export class SettingsDropdownV2Storybook extends PolymerElement {
  static get is() {
    return 'settings-dropdown-v2-storybook' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      basicDropdownValue_: {
        type: Number,
        value: 2,
      },

      basicDropdownDisabled_: {
        type: Boolean,
        value: false,
      },

      dropdownOptions_: {
        type: Array,
        value: () => {
          return [
            {label: 'Lion', value: 1},
            {label: 'Tiger', value: 2},
            {label: 'Bear', value: 3},
            {label: 'Dragon', value: 4},
          ];
        },
      },

      virtualManagedPref_: {
        type: Object,
        value: {
          key: 'virtual_managed_pref',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 2,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
        },
      },
    };
  }

  private basicDropdownDisabled_: boolean;
  private basicDropdownValue_: number;
  private dropdownOptions_: DropdownOptionList;
  private virtualManagedPref_: chrome.settingsPrivate.PrefObject<number>;

  private onBasicDropdownChange_(event: CustomEvent<number>): void {
    this.basicDropdownValue_ = event.detail;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsDropdownV2Storybook.is]: SettingsDropdownV2Storybook;
  }
}

customElements.define(
    SettingsDropdownV2Storybook.is, SettingsDropdownV2Storybook);
