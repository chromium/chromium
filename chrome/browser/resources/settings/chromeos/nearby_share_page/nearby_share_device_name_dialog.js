// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-device-name-dialog' allows editing of the device display name
 * when using Nearby Share.
 */
Polymer({
  is: 'nearby-share-device-name-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @type {nearby_share.NearbySettings} */
    settings: {
      type: Object,
    },

    /** @type {string} */
    errorMessage: {
      type: String,
      value: '',
    },
  },

  attached() {
    this.open();
  },

  open() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (!dialog.open) {
      dialog.showModal();
    }
  },

  close() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open) {
      dialog.close();
    }
  },

  /** @private */
  onDeviceNameInput_() {
    nearby_share.getNearbyShareSettings()
        .validateDeviceName(this.getEditInputValue_())
        .then((result) => {
          this.updateErrorMessage_(result.result);
        });
  },

  /** @private */
  onCancelTap_() {
    this.close();
  },

  /** @private */
  onDoneTap_() {
    nearby_share.getNearbyShareSettings()
        .setDeviceName(this.getEditInputValue_())
        .then((result) => {
          this.updateErrorMessage_(result.result);
          if (result.result ===
              nearbyShare.mojom.DeviceNameValidationResult.kValid) {
            this.close();
          }
        });
  },

  /**
   * @private
   *
   * @param {!nearbyShare.mojom.DeviceNameValidationResult} validationResult The
   *     error status from validating the provided device name.
   */
  updateErrorMessage_(validationResult) {
    switch (validationResult) {
      case nearbyShare.mojom.DeviceNameValidationResult.kErrorEmpty:
        this.errorMessage = this.i18n('nearbyShareDeviceNameEmptyError');
        break;
      case nearbyShare.mojom.DeviceNameValidationResult.kErrorTooLong:
        this.errorMessage = this.i18n('nearbyShareDeviceNameTooLongError');
        break;
      case nearbyShare.mojom.DeviceNameValidationResult.kErrorNotValidUtf8:
        this.errorMessage =
            this.i18n('nearbyShareDeviceNameInvalidCharactersError');
        break;
      default:
        this.errorMessage = '';
        break;
    }
  },

  /**
   * @private
   *
   * @return {!string}
   */
  getEditInputValue_() {
    return this.$$('cr-input').value;
  },

  /**
   * @private
   *
   * @param {!string} errorMessage The error message.
   * @return {boolean} Whether or not the error message exists.
   */
  hasErrorMessage_(errorMessage) {
    return errorMessage !== '';
  }
});
