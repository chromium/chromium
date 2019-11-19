// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './number_settings_section.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ScalingType} from '../data/scaling.js';

import {SelectBehavior} from './select_behavior.js';
import {SettingsBehavior} from './settings_behavior.js';

/*
 * Fit to page and fit to paper options will only be displayed for PDF
 * documents. If the custom option is selected, an additional input field will
 * appear to enter the custom scale factor.
 */
Polymer({
  is: 'print-preview-scaling-settings',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior, SelectBehavior],

  properties: {
    disabled: {
      type: Boolean,
      observer: 'onDisabledChanged_',
    },

    isPdf: Boolean,

    /** @private {string} */
    currentValue_: {
      type: String,
      observer: 'onInputChanged_',
    },

    /** @private {boolean} */
    customSelected_: {
      type: Boolean,
      computed: 'computeCustomSelected_(settingKey_, ' +
          'settings.scalingType.*, settings.scalingTypePdf.*)',
    },

    /** @private {boolean} */
    inputValid_: Boolean,

    /** @private {boolean} */
    dropdownDisabled_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    settingKey_: {
      type: String,
      computed: 'computeSettingKey_(isPdf)',
    },

    /** Mirroring the enum so that it can be used from HTML bindings. */
    ScalingValue: {
      type: Object,
      value: ScalingType,
    },
  },

  observers: [
    'onScalingTypeSettingChanged_(settingKey_, settings.scalingType.value, ' +
        'settings.scalingTypePdf.value)',
    'onScalingSettingChanged_(settings.scaling.value)',
  ],

  /** @private {string} */
  lastValidScaling_: '',

  /**
   * Whether the custom scaling setting has been set to true, but the custom
   * input has not yet been expanded. Used to determine whether changes in the
   * dropdown are due to user input or sticky settings.
   * @private {boolean}
   */
  customScalingSettingSet_: false,

  /**
   * Whether the user has selected custom scaling in the dropdown, but the
   * custom input has not yet been expanded. Used to determine whether to
   * auto-focus the custom input.
   * @private {boolean}
   */
  userSelectedCustomScaling_: false,

  onProcessSelectChange: function(value) {
    const isCustom = value === ScalingType.CUSTOM.toString();
    if (isCustom && !this.customScalingSettingSet_) {
      this.userSelectedCustomScaling_ = true;
    } else {
      this.customScalingSettingSet_ = false;
    }

    const valueAsNumber = parseInt(value, 10);
    if (isCustom || value === ScalingType.DEFAULT.toString()) {
      this.setSetting('scalingType', valueAsNumber);
    }
    if (this.isPdf ||
        this.getSetting('scalingTypePdf').value === ScalingType.DEFAULT ||
        this.getSetting('scalingTypePdf').value === ScalingType.CUSTOM) {
      this.setSetting('scalingTypePdf', valueAsNumber);
    }

    if (isCustom) {
      this.setSetting('scaling', this.currentValue_);
    }
  },

  /** @private */
  updateScalingToValid_: function() {
    if (!this.getSetting('scaling').valid) {
      this.currentValue_ = this.lastValidScaling_;
    } else {
      this.lastValidScaling_ = this.currentValue_;
    }
  },

  /**
   * Updates the input string when scaling setting is set.
   * @private
   */
  onScalingSettingChanged_: function() {
    const value = /** @type {string} */ (this.getSetting('scaling').value);
    this.lastValidScaling_ = value;
    this.currentValue_ = value;
  },

  /** @private */
  onScalingTypeSettingChanged_: function() {
    if (!this.settingKey_) {
      return;
    }

    const value = /** @type {!ScalingType} */
        (this.getSettingValue(this.settingKey_));
    if (value !== ScalingType.CUSTOM) {
      this.updateScalingToValid_();
    } else {
      this.customScalingSettingSet_ = true;
    }
    this.selectedValue = value.toString();
  },

  /**
   * Updates scaling settings based on the validity and current value of the
   * scaling input.
   * @private
   */
  onInputChanged_: function() {
    this.setSettingValid('scaling', this.inputValid_);

    if (this.currentValue_ !== '' && this.inputValid_ &&
        this.currentValue_ !== this.getSettingValue('scaling')) {
      this.setSetting('scaling', this.currentValue_);
    }
  },

  /** @private */
  onDisabledChanged_: function() {
    this.dropdownDisabled_ = this.disabled && this.inputValid_;
  },

  /**
   * @return {boolean} Whether the input should be disabled.
   * @private
   */
  inputDisabled_: function() {
    return !this.customSelected_ || this.dropdownDisabled_;
  },

  /**
   * @return {boolean} Whether the custom scaling option is selected.
   * @private
   */
  computeCustomSelected_: function() {
    return !!this.settingKey_ &&
        this.getSettingValue(this.settingKey_) === ScalingType.CUSTOM;
  },

  /**
   * @return {string} The key of the appropriate scaling setting.
   * @private
   */
  computeSettingKey_: function() {
    return this.isPdf ? 'scalingTypePdf' : 'scalingType';
  },

  /** @private */
  onCollapseChanged_: function() {
    if (this.customSelected_ && this.userSelectedCustomScaling_) {
      this.$$('print-preview-number-settings-section').getInput().focus();
    }
    this.customScalingSettingSet_ = false;
    this.userSelectedCustomScaling_ = false;
  },
});
