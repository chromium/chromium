// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/search_highlight_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import './print_preview_shared.css.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {stripDiacritics} from 'chrome://resources/js/search_highlight_utils.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {VendorCapability, VendorCapabilitySelectOption} from '../data/cdd.js';
import {getStringForCurrentLocale} from '../print_preview_utils.js';

import {getTemplate} from './advanced_settings_item.html.js';
import {updateHighlights} from './highlight_utils.js';
import {SettingsMixin} from './settings_mixin.js';

const PrintPreviewAdvancedSettingsItemElementBase =
    SettingsMixin(PolymerElement);

export class PrintPreviewAdvancedSettingsItemElement extends
    PrintPreviewAdvancedSettingsItemElementBase {
  static get is() {
    return 'print-preview-advanced-settings-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      capability: Object,
      currentValue_: String,
    };
  }

  static get observers() {
    return [
      'updateFromSettings_(capability, settings.vendorItems.value)',
    ];
  }

  capability: VendorCapability;
  private currentValue_: string;

  private updateFromSettings_() {
    const settings = this.getSetting('vendorItems').value;

    // The settings may not have a property with the id if they were populated
    // from sticky settings from a different destination or if the
    // destination's capabilities changed since the sticky settings were
    // generated.
    if (!settings.hasOwnProperty(this.capability.id)) {
      return;
    }

    const value = settings[this.capability.id];
    if (this.isCapabilityTypeSelect_()) {
      // Ignore a value that can't be selected.
      if (this.hasOptionWithValue_(value)) {
        this.currentValue_ = value;
      }
    } else {
      this.currentValue_ = value;
      this.shadowRoot!.querySelector('cr-input')!.value = this.currentValue_;
    }
  }

  /**
   * @return The display name for the setting.
   */
  private getDisplayName_(item: VendorCapability|
                          VendorCapabilitySelectOption): string {
    let displayName = item.display_name;
    if (!displayName && item.display_name_localized) {
      displayName = getStringForCurrentLocale(item.display_name_localized!);
    }
    return displayName || '';
  }

  /**
   * @return Whether the capability represented by this item is of type select.
   */
  private isCapabilityTypeSelect_(): boolean {
    return this.capability.type === 'SELECT';
  }

  /**
   * @return Whether the capability represented by this item is of type
   *     checkbox.
   */
  private isCapabilityTypeCheckbox_(): boolean {
    return this.capability.type === 'TYPED_VALUE' &&
        this.capability.typed_value_cap!.value_type === 'BOOLEAN';
  }

  /**
   * @return Whether the capability represented by this item is of type input.
   */
  private isCapabilityTypeInput_(): boolean {
    return !this.isCapabilityTypeSelect_() && !this.isCapabilityTypeCheckbox_();
  }

  /**
   * @return Whether the checkbox setting is checked.
   */
  private isChecked_(): boolean {
    return this.currentValue_ === 'true';
  }

  /**
   * @param option The option for a select capability.
   * @return Whether the option is selected.
   */
  private isOptionSelected_(option: VendorCapabilitySelectOption): boolean {
    return this.currentValue_ === undefined ?
        !!option.is_default :
        option.value === this.currentValue_;
  }

  /**
   * @return The placeholder value for the capability's text input.
   */
  private getCapabilityPlaceholder_(): string {
    if (this.capability.type === 'TYPED_VALUE' &&
        this.capability.typed_value_cap &&
        this.capability.typed_value_cap!.default !== undefined) {
      return this.capability.typed_value_cap!.default.toString() || '';
    }
    if (this.capability.type === 'RANGE' && this.capability.range_cap &&
        this.capability.range_cap!.default !== undefined) {
      return this.capability.range_cap!.default.toString() || '';
    }
    return '';
  }

  private hasOptionWithValue_(value: string): boolean {
    return !!this.capability.select_cap &&
        !!this.capability.select_cap!.option &&
        this.capability.select_cap!.option.some(
            option => option.value === value);
  }

  /**
   * @param query The current search query.
   * @return Whether the item has a match for the query.
   */
  hasMatch(query: RegExp|null): boolean {
    if (!query) {
      return true;
    }

    const strippedCapabilityName =
        stripDiacritics(this.getDisplayName_(this.capability));
    if (strippedCapabilityName.match(query)) {
      return true;
    }

    if (!this.isCapabilityTypeSelect_()) {
      return false;
    }

    for (const option of this.capability.select_cap!.option!) {
      const strippedOptionName = stripDiacritics(this.getDisplayName_(option));
      if (strippedOptionName.match(query)) {
        return true;
      }
    }
    return false;
  }

  private onUserInput_(e: Event) {
    this.currentValue_ = (e.target! as CrInputElement).value;
  }

  private onCheckboxInput_(e: Event) {
    this.currentValue_ =
        (e.target! as CrCheckboxElement).checked ? 'true' : 'false';
  }

  /**
   * @return The current value of the setting, or the empty string if it is not
   *     set.
   */
  getCurrentValue(): string {
    return this.currentValue_ || '';
  }

  /**
   * Only used in tests.
   * @param value A value to set the setting to.
   */
  setCurrentValueForTest(value: string) {
    this.currentValue_ = value;
  }

  /**
   * @return The highlight wrappers and that were created.
   */
  updateHighlighting(query: RegExp|null, bubbles: Map<HTMLElement, number>):
      HTMLElement[] {
    return updateHighlights(this, query, bubbles);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-advanced-settings-item':
        PrintPreviewAdvancedSettingsItemElement;
  }
}

customElements.define(
    PrintPreviewAdvancedSettingsItemElement.is,
    PrintPreviewAdvancedSettingsItemElement);
