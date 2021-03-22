// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-onboarding-page' component handles the Nearby Share
 * onboarding flow. It is embedded in chrome://os-settings, chrome://settings
 * and as a standalone dialog via chrome://nearby.
 */
Polymer({
  is: 'nearby-onboarding-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?nearby_share.NearbySettings} */
    settings: {
      type: Object,
    },

    /** @type {string} */
    errorMessage: {
      type: String,
      value: '',
    },
  },

  listeners: {
    'next': 'onNext_',
    'close': 'onClose_',
    'keydown': 'onKeydown_',
    'view-enter-start': 'onViewEnterStart_',
  },


  /** @private */
  onNext_() {
    this.submitDeviceNameInput_();
  },

  /** @private */
  onClose_() {
    processOnboardingCancelledMetrics(
        NearbyShareOnboardingFinalState.DEVICE_NAME_PAGE);
    this.fire('onboarding-cancelled');
  },

  /**
   * @param {!KeyboardEvent} e Event containing the key
   * @private
   */
  onKeydown_(e) {
    e.stopPropagation();
    if (e.key === 'Enter') {
      this.submitDeviceNameInput_();
      e.preventDefault();
    }
  },

  /** @private */
  onViewEnterStart_() {
    this.$$('#deviceName').focus();
    processOnboardingInitiatedMetrics(new URL(document.URL));
  },


  /** @private */
  onDeviceNameInput_() {
    nearby_share.getNearbyShareSettings()
        .validateDeviceName(this.$.deviceName.value)
        .then((result) => {
          this.updateErrorMessage_(result.result);
        });
  },

  /** @private */
  submitDeviceNameInput_() {
    nearby_share.getNearbyShareSettings()
        .setDeviceName(this.$.deviceName.value)
        .then((result) => {
          this.updateErrorMessage_(result.result);
          if (result.result ===
              nearbyShare.mojom.DeviceNameValidationResult.kValid) {
            this.fire('change-page', {page: 'visibility'});
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
   * @param {!string} errorMessage The error message.
   * @return {boolean} Whether or not the error message exists.
   */
  hasErrorMessage_(errorMessage) {
    return errorMessage !== '';
  }
});
