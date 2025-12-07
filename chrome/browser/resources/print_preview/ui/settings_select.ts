// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CapabilityWithReset, SelectOption} from '../data/cdd.js';
import type {Settings} from '../data/model.js';
import {getStringForCurrentLocale} from '../print_preview_utils.js';

import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';
import {getCss} from './settings_select.css.js';
import {getHtml} from './settings_select.html.js';

const PrintPreviewSettingsSelectElementBase =
    SettingsMixin(SelectMixin(CrLitElement));

export class PrintPreviewSettingsSelectElement extends
    PrintPreviewSettingsSelectElementBase {
  static get is() {
    return 'print-preview-settings-select';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ariaLabel: {type: String},
      capability: {type: Object},
      settingName: {type: String},
      disabled: {type: Boolean},
    };
  }

  override accessor ariaLabel: string = '';
  accessor capability: CapabilityWithReset&{option: SelectOption[]}|null = null;
  accessor settingName: keyof Settings = 'dpi';
  accessor disabled: boolean = false;

  /**
   * @param option Option to check.
   * @return Whether the option is selected.
   */
  protected isSelected_(option: SelectOption): boolean {
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
  protected getValue_(option: SelectOption): string {
    return JSON.stringify(option);
  }

  /**
   * @param option Option to get the display name for.
   * @return Display name for the option.
   */
  protected getDisplayName_(option: SelectOption): string {
    let displayName = option.custom_display_name;
    if (!displayName && option.custom_display_name_localized) {
      displayName =
          getStringForCurrentLocale(option.custom_display_name_localized);
    }
    return displayName || option.name || '';
  }

  override onProcessSelectChange(value: string) {
    let newValue = null;
    try {
      newValue = JSON.parse(value);
    } catch (e) {
      assertNotReached();
    }
    if (value !== JSON.stringify(this.getSettingValue(this.settingName))) {
      this.setSetting(this.settingName, newValue);
    }
  }
}

export type SettingsSelectElement = PrintPreviewSettingsSelectElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-settings-select': PrintPreviewSettingsSelectElement;
  }
}

customElements.define(
    PrintPreviewSettingsSelectElement.is, PrintPreviewSettingsSelectElement);
