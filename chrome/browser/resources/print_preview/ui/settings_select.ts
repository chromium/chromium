// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select_css.m.js';
import './print_preview_shared_css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SelectOption} from '../data/cdd.js';
import {getStringForCurrentLocale} from '../print_preview_utils.js';

import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

const PrintPreviewSettingsSelectElementBase =
    SettingsMixin(SelectMixin(PolymerElement));

export class PrintPreviewSettingsSelectElement extends
    PrintPreviewSettingsSelectElementBase {
  static get is() {
    return 'print-preview-settings-select';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      ariaLabel: String,

      capability: Object,

      settingName: String,

      disabled: Boolean,
    };
  }

  ariaLabel: string;
  capability: SelectOption[];
  settingName: string;
  disabled: boolean;

  /**
   * @param option Option to check.
   * @return Whether the option is selected.
   */
  private isSelected_(option: SelectOption): boolean {
    return this.getValue_(option) === this.selectedValue ||
        (!!option.is_default && this.selectedValue === '');
  }

  selectValue(value: string) {
    this.selectedValue = value;
  }

  /**
   * @param option Option to get the value for.
   * @return Value for the option.
   */
  private getValue_(option: SelectOption): string {
    return JSON.stringify(option);
  }

  /**
   * @param option Option to get the display name for.
   * @return Display name for the option.
   */
  private getDisplayName_(option: SelectOption): string {
    let displayName = option.custom_display_name;
    if (!displayName && option.custom_display_name_localized) {
      displayName = getStringForCurrentLocale(
          assert(option.custom_display_name_localized));
    }
    return displayName || option.name || '';
  }

  onProcessSelectChange(value: string) {
    let newValue = null;
    try {
      newValue = JSON.parse(value);
    } catch (e) {
      assertNotReached();
      return;
    }
    if (value !== JSON.stringify(this.getSettingValue(this.settingName))) {
      this.setSetting(this.settingName, newValue);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-settings-select': PrintPreviewSettingsSelectElement;
  }
}

customElements.define(
    PrintPreviewSettingsSelectElement.is, PrintPreviewSettingsSelectElement);
