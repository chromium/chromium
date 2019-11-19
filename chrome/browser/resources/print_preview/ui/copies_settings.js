// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import './number_settings_section.js';
import './print_preview_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-copies-settings',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior],

  properties: {
    /** @private {string} */
    currentValue_: {
      type: String,
      observer: 'onInputChanged_',
    },

    /** @private {boolean} */
    inputValid_: Boolean,

    disabled: Boolean,
  },

  observers:
      ['onSettingsChanged_(settings.copies.value, settings.collate.value)'],

  /**
   * Updates the input string when the setting has been initialized.
   * @private
   */
  onSettingsChanged_: function() {
    const copies = this.getSetting('copies');
    if (this.inputValid_) {
      this.currentValue_ = /** @type {string} */ (copies.value.toString());
    }
    const collate = this.getSetting('collate');
    this.$.collate.checked = /** @type {boolean} */ (collate.value);
  },

  /**
   * Updates model.copies and model.copiesInvalid based on the validity
   * and current value of the copies input.
   * @private
   */
  onInputChanged_: function() {
    if (this.currentValue_ !== '' &&
        this.currentValue_ !== this.getSettingValue('copies').toString()) {
      this.setSetting(
          'copies', this.inputValid_ ? parseInt(this.currentValue_, 10) : 1);
    }
    this.setSettingValid('copies', this.inputValid_);
  },

  /**
   * @return {boolean} Whether collate checkbox should be hidden.
   * @private
   */
  collateHidden_: function() {
    return !this.inputValid_ || this.currentValue_ === '' ||
        parseInt(this.currentValue_, 10) == 1;
  },

  /** @private */
  onCollateChange_: function() {
    this.setSetting('collate', this.$.collate.checked);
  },
});
