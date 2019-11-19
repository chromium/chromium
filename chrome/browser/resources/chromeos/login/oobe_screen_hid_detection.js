// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe HID detection screen implementation.
 */

login.createScreen('HIDDetectionScreen', 'hid-detection', function() {
  return {
    // Enumeration of possible connection states of a device.
    CONNECTION: {
      SEARCHING: 'searching',
      USB: 'usb',
      CONNECTED: 'connected',
      PAIRING: 'pairing',
      PAIRED: 'paired',
      // Special info state.
      UPDATE: 'update',
    },

    EXTERNAL_API: [
      'setKeyboardState',
      'setMouseState',
      'setKeyboardPinCode',
      'setNumKeysEnteredExpected',
      'setNumKeysEnteredPincode',
      'setMouseDeviceName',
      'setKeyboardDeviceName',
      'setKeyboardDeviceLabel',
      'setContinueButtonEnabled',
    ],

    /**
     * Button to move to usual OOBE flow after detection.
     * @private
     */
    continueButton_: null,

    /** @type {string} */
    keyboardState_: '',

    /** @type {string} */
    keyboardPinCode_: '',

    /** @type {boolean} */
    keyboardEnteredExpected_: false,

    /** @type {number} */
    numKeysEnteredPincode_: 0,

    /** @type {string} */
    keyboardDeviceLabel_: '',

    setKeyboardState: function(stateId) {
      this.keyboardState_ = stateId;

      this.updatePincodeKeysState_();
      if (stateId === undefined)
        return;
      $('oobe-hid-detection-md').setKeyboardState(stateId);
      if (stateId == this.CONNECTION.PAIRED) {
        $('oobe-hid-detection-md').keyboardPairedLabel =
            this.keyboardDeviceLabel_;
      } else if (stateId == this.CONNECTION.PAIRING) {
        $('oobe-hid-detection-md').keyboardPairingLabel =
            this.keyboardDeviceLabel_;
      }
    },

    setMouseState: function(stateId) {
      if (stateId === undefined)
        return;
      $('oobe-hid-detection-md').setMouseState(stateId);
    },

    setKeyboardPinCode: function(value) {
      this.keyboardPinCode_ = value;
      this.updatePincodeKeysState_();
    },

    setNumKeysEnteredExpected: function(value) {
      this.keyboardEnteredExpected_ = value;
      this.updatePincodeKeysState_();
    },

    setNumKeysEnteredPincode: function(value) {
      this.numKeysEnteredPincode_ = value;
      this.updatePincodeKeysState_();
    },

    setMouseDeviceName: function(value) {},

    setKeyboardDeviceName: function(value) {},

    setKeyboardDeviceLabel: function(value) {
      this.keyboardDeviceLabel_ = value;
    },

    setContinueButtonEnabled: function(enabled) {
      $('oobe-hid-detection-md').continueButtonDisabled = !enabled;
    },

    /** @override */
    decorate: function() {
      $('oobe-hid-detection-md').screen = this;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('oobe-hid-detection-md');
    },

    /**
     * Sets state for mouse-block.
     * @param {state} one of keys of this.CONNECTION dict.
     */
    setPointingDeviceState: function(state) {
      if (state === undefined)
        return;
      $('oobe-hid-detection-md').setMouseState(state);
    },

    /**
     * Updates state for pincode key elements.
     */
    updatePincodeKeysState_: function() {
      var pincode = this.keyboardPinCode_;
      var state = this.keyboardState_;
      var label = this.keyboardDeviceLabel_;
      var entered = this.numKeysEnteredPincode_;
      // Whether the functionality of getting num of entered keys is available.
      var expected = this.keyboardEnteredExpected_;

      $('oobe-hid-detection-md').setKeyboardState(state);
      $('oobe-hid-detection-md')
          .setPincodeState(pincode, entered, expected, label);
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      $('oobe-hid-detection-md').setMouseState(this.CONNECTION.SEARCHING);
      $('oobe-hid-detection-md').setKeyboardState(this.CONNECTION.SEARCHING);
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      $('oobe-hid-detection-md').i18nUpdateLocale();
    },

  };
});
