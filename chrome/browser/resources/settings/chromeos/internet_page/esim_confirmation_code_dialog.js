// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to enter a confirmation code if required when
 * installing an eSIM profile.
 */

Polymer({
  is: 'esim-confirmation-code-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @type {?chromeos.cellularSetup.mojom.ESimProfileRemote} */
    profile: {
      type: Object,
      value: null,
    },

    /** @private {string} */
    confirmationCode_: {
      type: String,
      value: '',
      observer: 'onConfirmationCodeChanged_',
    },

    /** @private {boolean} */
    isInstallInProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showError_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onConfirmationCodeChanged_() {
    this.showError_ = false;
  },

  /**
   * @param {Event} event
   * @private
   */
  onDoneClicked_(event) {
    this.isInstallInProgress_ = true;
    this.showError_ = false;

    this.profile.installProfile(this.confirmationCode_).then((response) => {
      this.isInstallInProgress_ = false;
      if (response.result ===
          chromeos.cellularSetup.mojom.ESimOperationResult.kSuccess) {
        this.$.confirmationCodeDialog.close();
        return;
      }
      this.showError_ = true;
    });
  },

  /**
   * @param {Event} event
   * @private
   */
  onCancelClicked_(event) {
    this.$.confirmationCodeDialog.close();
  },

  /** @private */
  isDoneButtonDisabled_() {
    return !this.confirmationCode_ || this.isInstallInProgress_;
  },
});
