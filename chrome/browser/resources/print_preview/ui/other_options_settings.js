// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import './print_preview_shared_css.js';
import './settings_section.js';
import '../strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-other-options-settings',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior, I18nBehavior],

  properties: {
    disabled: Boolean,

    /**
     * @private {!Array<!{name: string,
     *                    label: string,
     *                    value: (boolean | undefined),
     *                    managed: (boolean | undefined),
     *                    available: (boolean | undefined)}>}
     */
    options_: {
      type: Array,
      value: function() {
        return [
          {name: 'headerFooter', label: 'optionHeaderFooter'},
          {name: 'cssBackground', label: 'optionBackgroundColorsAndImages'},
          {name: 'rasterize', label: 'optionRasterize'},
          {name: 'selectionOnly', label: 'optionSelectionOnly'},
        ];
      },
    },

    /**
     * The index of the checkbox that should display the "Options" title.
     * @private {number}
     */
    firstIndex_: {
      type: Number,
      value: 0,
    },
  },

  observers: [
    'onHeaderFooterSettingChange_(settings.headerFooter.*)',
    'onCssBackgroundSettingChange_(settings.cssBackground.*)',
    'onRasterizeSettingChange_(settings.rasterize.*)',
    'onSelectionOnlySettingChange_(settings.selectionOnly.*)',
  ],

  /** @private {!Map<string, ?number>} */
  timeouts_: new Map(),

  /** @private {!Map<string, boolean>} */
  previousValues_: new Map(),

  /**
   * @param {string} settingName The name of the setting to updated.
   * @param {boolean} newValue The new value for the setting.
   */
  updateSettingWithTimeout_: function(settingName, newValue) {
    const timeout = this.timeouts_.get(settingName);
    if (timeout != null) {
      clearTimeout(timeout);
    }

    this.timeouts_.set(
        settingName, setTimeout(() => {
          this.timeouts_.delete(settingName);
          if (this.previousValues_.get(settingName) == newValue) {
            return;
          }
          this.previousValues_.set(settingName, newValue);
          this.setSetting(settingName, newValue);

          // For tests only
          this.fire('update-checkbox-setting', settingName);
        }, 200));
  },

  /**
   * @param {number} index The index of the option to update.
   * @private
   */
  updateOptionFromSetting_: function(index) {
    const setting = this.getSetting(this.options_[index].name);
    this.set(`options_.${index}.available`, setting.available);
    this.set(`options_.${index}.value`, setting.value);
    this.set(`options_.${index}.managed`, setting.setByPolicy);

    // Update first index
    const availableOptions = this.options_.filter(option => !!option.available);
    if (availableOptions.length > 0) {
      this.firstIndex_ = this.options_.indexOf(availableOptions[0]);
    }
  },

  /**
   * @param {boolean} managed Whether the setting is managed by policy.
   * @param {boolean} disabled value of this.disabled
   * @return {boolean} Whether the checkbox should be disabled.
   * @private
   */
  getDisabled_: function(managed, disabled) {
    return managed || disabled;
  },

  /** @private */
  onHeaderFooterSettingChange_: function() {
    this.updateOptionFromSetting_(0);
  },

  /** @private */
  onCssBackgroundSettingChange_: function() {
    this.updateOptionFromSetting_(1);
  },

  /** @private */
  onRasterizeSettingChange_: function() {
    this.updateOptionFromSetting_(2);
  },

  /** @private */
  onSelectionOnlySettingChange_: function() {
    this.updateOptionFromSetting_(3);
  },

  /**
   * @param {!Event} e Contains the checkbox item that was checked.
   * @private
   */
  onChange_: function(e) {
    const name = e.model.item.name;
    this.updateSettingWithTimeout_(name, this.$$(`#${name}`).checked);
  },

  /**
   * @param {number} index The index of the settings section.
   * @return {string} Class string containing 'first-visible' if the settings
   *     section is the first visible.
   * @private
   */
  getClass_: function(index) {
    return index === this.firstIndex_ ? 'first-visible' : '';
  },
});
