// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element handling errors when installing an eSIM
 * profile, such as requiring a confirmation code.
 */

Polymer({
  is: 'esim-install-error-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * The error code returned when profile install attempt was made in networks
     * list.
     * @type {?chromeos.cellularSetup.mojom.ProfileInstallResult}
     */
    errorCode: {
      type: Object,
      value: null,
    },

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
    isConfirmationCodeInvalid_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onConfirmationCodeChanged_() {
    this.isConfirmationCodeInvalid_ = false;
  },

  /**
   * @param {Event} event
   * @private
   */
  onDoneClicked_(event) {
    if (!this.isConfirmationCodeError_()) {
      this.$.installErrorDialog.close();
      return;
    }
    this.isInstallInProgress_ = true;
    this.isConfirmationCodeInvalid_ = false;

    this.profile.installProfile(this.confirmationCode_).then((response) => {
      this.isInstallInProgress_ = false;
      if (response.result ===
          chromeos.cellularSetup.mojom.ESimOperationResult.kSuccess) {
        this.$.installErrorDialog.close();
        return;
      }
      // TODO(crbug.com/1093185) Only display confirmation code entry if the
      // error was an invalid confirmation code, else display generic error.
      this.isConfirmationCodeInvalid_ = true;
    });
  },

  /**
   * @param {Event} event
   * @private
   */
  onCancelClicked_(event) {
    this.$.installErrorDialog.close();
  },

  /**
   * @return {boolean}
   * @private
   */
  /** @private */
  isConfirmationCodeError_() {
    return this.errorCode ===
        chromeos.cellularSetup.mojom.ProfileInstallResult
            .kErrorNeedsConfirmationCode;
  },

  /**
   * @return {boolean}
   * @private
   */
  isDoneButtonDisabled_() {
    return this.isConfirmationCodeError_() &&
        (!this.confirmationCode_ || this.isInstallInProgress_);
  },
});
