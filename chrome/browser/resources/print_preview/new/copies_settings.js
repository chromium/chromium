// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-copies-settings',

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
    if (this.inputValid_)
      this.currentValue_ = /** @type {string} */ (copies.value.toString());
    const collate = this.getSetting('collate');
    this.$.collate.checked = /** @type {boolean} */ (collate.value);
  },

  /**
   * Updates model.copies and model.copiesInvalid based on the validity
   * and current value of the copies input.
   * @private
   */
  onInputChanged_: function() {
    if (this.currentValue_ !== '') {
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
