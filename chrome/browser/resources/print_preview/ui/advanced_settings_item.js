// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/search_highlight_style_css.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import './print_preview_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {VendorCapability, VendorCapabilitySelectOption} from '../data/cdd.js';
import {Destination} from '../data/destination.js';
import {getStringForCurrentLocale} from '../print_preview_utils.js';

import {updateHighlights} from './highlight_utils.js';
import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-advanced-settings-item',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior],

  properties: {
    /** @type {!VendorCapability} */
    capability: Object,

    /** @private {string} */
    currentValue_: String,
  },

  observers: [
    'updateFromSettings_(capability, settings.vendorItems.value)',
  ],

  /** @private */
  updateFromSettings_() {
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
      this.$$('cr-input').value = this.currentValue_;
    }
  },

  /**
   * @param {!VendorCapability |
   *         !VendorCapabilitySelectOption} item
   * @return {string} The display name for the setting.
   * @private
   */
  getDisplayName_(item) {
    let displayName = item.display_name;
    if (!displayName && item.display_name_localized) {
      displayName = getStringForCurrentLocale(item.display_name_localized);
    }
    return displayName || '';
  },

  /**
   * @return {boolean} Whether the capability represented by this item is
   *     of type select.
   * @private
   */
  isCapabilityTypeSelect_() {
    return this.capability.type === 'SELECT';
  },

  /**
   * @return {boolean} Whether the capability represented by this item is
   *     of type checkbox.
   * @private
   */
  isCapabilityTypeCheckbox_() {
    return this.capability.type === 'TYPED_VALUE' &&
        this.capability.typed_value_cap.value_type === 'BOOLEAN';
  },

  /**
   * @return {boolean} Whether the capability represented by this item is
   *     of type input.
   * @private
   */
  isCapabilityTypeInput_() {
    return !this.isCapabilityTypeSelect_() && !this.isCapabilityTypeCheckbox_();
  },

  /**
   * @return {boolean} Whether the checkbox setting is checked.
   * @private
   */
  isChecked_() {
    return this.currentValue_ === 'true';
  },

  /**
   * @param {!VendorCapabilitySelectOption} option The option
   *     for a select capability.
   * @return {boolean} Whether the option is selected.
   * @private
   */
  isOptionSelected_(option) {
    return this.currentValue_ === undefined ?
        !!option.is_default :
        option.value === this.currentValue_;
  },

  /**
   * @return {string} The placeholder value for the capability's text input.
   * @private
   */
  getCapabilityPlaceholder_() {
    if (this.capability.type === 'TYPED_VALUE' &&
        this.capability.typed_value_cap &&
        this.capability.typed_value_cap.default !== undefined) {
      return this.capability.typed_value_cap.default.toString() || '';
    }
    if (this.capability.type === 'RANGE' && this.capability.range_cap &&
        this.capability.range_cap.default !== undefined) {
      return this.capability.range_cap.default.toString() || '';
    }
    return '';
  },

  /**
   * @return {boolean}
   * @private
   */
  hasOptionWithValue_(value) {
    return !!this.capability.select_cap &&
        !!this.capability.select_cap.option &&
        this.capability.select_cap.option.some(
            option => option.value === value);
  },

  /**
   * @param {?RegExp} query The current search query.
   * @return {boolean} Whether the item has a match for the query.
   */
  hasMatch(query) {
    if (!query || this.getDisplayName_(this.capability).match(query)) {
      return true;
    }

    if (!this.isCapabilityTypeSelect_()) {
      return false;
    }

    for (const option of
         /** @type {!Array<!VendorCapabilitySelectOption>} */ (
             this.capability.select_cap.option)) {
      if (this.getDisplayName_(option).match(query)) {
        return true;
      }
    }
    return false;
  },

  /**
   * @param {!Event} e Event containing the new value.
   * @private
   */
  onUserInput_(e) {
    this.currentValue_ = e.target.value;
  },

  /**
   * @param {!Event} e Event containing the new value.
   * @private
   */
  onCheckboxInput_(e) {
    this.currentValue_ = e.target.checked ? 'true' : 'false';
  },

  /**
   * @return {string} The current value of the setting, or the empty string if
   *     it is not set.
   */
  getCurrentValue() {
    return this.currentValue_ || '';
  },

  /**
   * Only used in tests.
   * @param {string} value A value to set the setting to.
   */
  setCurrentValueForTest(value) {
    this.currentValue_ = value;
  },

  /**
   * @param {?RegExp} query The current search query.
   * @param {!Map<!Node, number>} bubbles
   * @return {!Array<!Node>} The highlight wrappers and that were created.
   */
  updateHighlighting(query, bubbles) {
    return updateHighlights(this, query, bubbles);
  },
});
