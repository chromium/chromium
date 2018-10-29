// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');

/** @enum {number} */
print_preview_new.ScalingState = {
  INIT: 0,
  VALID: 1,
  INVALID: 2,
  FIT_TO_PAGE: 3,
};

/*
 * When fit to page is available, the checkbox and input interact as follows:
 * 1. When checkbox is checked, the fit to page scaling value is displayed in
 * the input. The error message is cleared if it was present.
 * 2. When checkbox is unchecked, the most recent valid scale value is restored.
 * 3. If the input is modified while the checkbox is checked, the checkbox will
 * be unchecked automatically, regardless of the validity of the new value.
 */
Polymer({
  is: 'print-preview-scaling-settings',

  behaviors: [SettingsBehavior],

  properties: {
    /** @type {Object} */
    documentInfo: Object,

    /** @private {string} */
    currentValue_: {
      type: String,
      observer: 'onInputChanged_',
    },

    /** @private {boolean} */
    inputValid_: Boolean,

    /** @private {boolean} */
    hideInput_: Boolean,

    disabled: Boolean,

    /** @private {!print_preview_new.ScalingState} */
    currentState_: {
      type: Number,
      value: print_preview_new.ScalingState.INIT,
      observer: 'onStateChange_',
    },
  },

  observers: [
    'onFitToPageSettingChange_(settings.fitToPage.value)',
    'onFitToPageScalingSet_(documentInfo.fitToPageScaling)',
    'onScalingSettingChanged_(settings.scaling.value)',
    'onScalingValidChanged_(settings.scaling.valid)',
  ],

  /**
   * Timeout used to delay processing of the checkbox input.
   * @private {?number}
   */
  fitToPageTimeout_: null,

  /** @private {boolean} */
  ignoreFtp_: false,

  /** @private {boolean} */
  ignoreValid_: false,

  /** @private {boolean} */
  ignoreValue_: false,

  /** @private {string} */
  lastValidScaling_: '100',

  /** @private {?boolean} */
  lastFitToPageValue_: null,

  /** @private */
  onFitToPageSettingChange_: function() {
    if (this.ignoreFtp_ || !this.getSetting('fitToPage').available)
      return;

    const fitToPage = this.getSetting('fitToPage').value;

    if (fitToPage) {
      this.currentState_ = print_preview_new.ScalingState.FIT_TO_PAGE;
      return;
    }

    this.currentState_ = this.getSetting('scaling').valid ?
        print_preview_new.ScalingState.VALID :
        print_preview_new.ScalingState.INVALID;
  },

  /**
   * @return {string} The value to display for fit to page scling.
   * @private
   */
  getFitToPageScalingDisplayValue_: function() {
    return this.documentInfo.fitToPageScaling > 0 ?
        this.documentInfo.fitToPageScaling.toString() :
        '';
  },

  /** @private */
  onFitToPageScalingSet_: function() {
    if (this.currentState_ != print_preview_new.ScalingState.FIT_TO_PAGE)
      return;

    this.ignoreValue_ = true;
    this.currentValue_ = this.getFitToPageScalingDisplayValue_();
    this.ignoreValue_ = false;
  },

  /**
   * Updates the input string when scaling setting is set.
   * @private
   */
  onScalingSettingChanged_: function() {
    // Update last valid scaling and ensure input string matches.
    this.lastValidScaling_ =
        /** @type {string} */ (this.getSetting('scaling').value);
    this.currentValue_ = this.lastValidScaling_;
    this.currentState_ = print_preview_new.ScalingState.VALID;
  },

  /**
   * Updates the state of the UI when scaling validity is set.
   * @private
   */
  onScalingValidChanged_: function() {
    if (this.ignoreValid_)
      return;

    this.currentState_ = this.getSetting('scaling').valid ?
        print_preview_new.ScalingState.VALID :
        print_preview_new.ScalingState.INVALID;
  },

  /**
   * Updates scaling and fit to page settings based on the validity and current
   * value of the scaling input.
   * @private
   */
  onInputChanged_: function() {
    if (this.ignoreValue_)
      return;

    if (this.currentValue_ !== '')
      this.setSettingValid('scaling', this.inputValid_);

    if (this.currentValue_ !== '' && this.inputValid_)
      this.setSetting('scaling', this.currentValue_);
  },

  /** @private */
  onFitToPageChange_: function() {
    const newValue = this.$$('#fit-to-page-checkbox').checked;

    if (this.fitToPageTimeout_ !== null)
      clearTimeout(this.fitToPageTimeout_);

    this.fitToPageTimeout_ = setTimeout(() => {
      this.fitToPageTimeout_ = null;

      if (newValue === this.lastFitToPageValue_)
        return;

      this.lastFitToPageValue_ = newValue;
      this.setSetting('fitToPage', newValue);

      if (newValue == false)
        this.currentValue_ = this.lastValidScaling_;
      else
        this.currentState_ = print_preview_new.ScalingState.FIT_TO_PAGE;

      // For tests only
      this.fire('update-checkbox-setting', 'fitToPage');
    }, 200);
  },

  /**
   * @return {boolean} Whether the input should be disabled.
   * @private
   */
  getDisabled_: function() {
    return this.disabled &&
        this.currentState_ !== print_preview_new.ScalingState.INVALID;
  },

  /**
   * @param {!print_preview_new.ScalingState} current
   * @param {!print_preview_new.ScalingState} previous
   * @private
   */
  onStateChange_: function(current, previous) {
    if (previous == print_preview_new.ScalingState.FIT_TO_PAGE) {
      this.ignoreFtp_ = true;
      this.$$('#fit-to-page-checkbox').checked = false;
      this.lastFitToPageValue_ = false;
      if (current == print_preview_new.ScalingState.VALID)
        this.setSetting('fitToPage', false);
      this.ignoreFtp_ = false;
    }
    if (current == print_preview_new.ScalingState.FIT_TO_PAGE) {
      if (previous == print_preview_new.ScalingState.INVALID) {
        this.ignoreValid_ = true;
        this.setSettingValid('scaling', true);
        this.ignoreValid_ = false;
      }
      this.$$('#fit-to-page-checkbox').checked = true;
      this.ignoreValue_ = true;
      this.currentValue_ = this.getFitToPageScalingDisplayValue_();
      this.ignoreValue_ = false;
    }
    if (current == print_preview_new.ScalingState.VALID &&
        previous == print_preview_new.ScalingState.INVALID &&
        this.getSetting('fitToPage').available) {
      this.setSetting('fitToPage', false);
    }

  },

  /**
   * @return {string} The label to use on the scaling input.
   * @private
   */
  getScalingInputLabel_: function() {
    return this.getSetting('fitToPage').available ?
        '' :
        loadTimeData.getString('scalingLabel');
  },
});
