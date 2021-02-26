// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design HID detection
 * screen.
 */

(function() {
/** @const {number} */ var PINCODE_LENGTH = 6;

// Enumeration of possible connection states of a device.
const CONNECTION = {
  SEARCHING: 'searching',
  USB: 'usb',
  CONNECTED: 'connected',
  PAIRING: 'pairing',
  PAIRED: 'paired',
};

Polymer({
  is: 'hid-detection-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'setKeyboardState',
    'setMouseState',
    'setKeyboardPinCode',
    'setPinDialogVisible',
    'setNumKeysEnteredPinCode',
    'setPointingDeviceName',
    'setKeyboardDeviceName',
    'setTouchscreenDetectedState',
    'setContinueButtonEnabled',
  ],

  properties: {
    /** "Continue" button is disabled until HID devices are paired. */
    continueButtonEnabled: {
      type: Boolean,
      value: false,
    },

    /**
     * The keyboard device name
     */
    keyboardDeviceName: {
      type: String,
      value: '',
    },

    /**
     * The pointing device name
     */
    pointingDeviceName: {
      type: String,
      value: '',
    },

    /**
     * State of touchscreen detection
     * @private
     */
    touchscreenDetected_: {
      type: Boolean,
      value: false,
    },

    /**
     * Current state in mouse pairing process.
     * @private
     */
    mouseState_: {
      type: String,
      value: CONNECTION.SEARCHING,
    },

    /**
     * Current state in keyboard pairing process.
     * @private
     */
    keyboardState_: {
      type: String,
      value: CONNECTION.SEARCHING,
    },

    /**
     * Controls the visibility of the PIN dialog.
     * @private
     */
    pinDialogVisible_: {
      type: Boolean,
      value: false,
      observer: 'onPinDialogVisibilityChanged_',
    },

    /**
     * The PIN code to be typed by the user
     */
    pinCode: {
      type: String,
      value: '000000',
      observer: 'onPinParametersChanged_',
    },

    /**
     * The number of keys that the user already entered for this PIN.
     * This helps the user to see what's the next key to be pressed.
     */
    numKeysEnteredPinCode: {
      type: Number,
      value: 0,
      observer: 'onPinParametersChanged_',
    },

    /**
     *  Whether the dialog for PIN input is being shown.
     *  Internal use only. Used for preventing multiple openings.
     */
    pinDialogIsOpen_: {
      type: Boolean,
      value: false,
    },

    /**
     * The title that is displayed on the PIN dialog
     */
    pinDialogTitle: {
      type: String,
      computed: 'getPinDialogTitle_(locale, keyboardDeviceName)',
    },
  },

  /** @override */
  ready() {
    this.initializeLoginScreen('HIDDetectionScreen', {
      resetAllowed: false,
    });
  },

  getPrerequisitesText_(locale, touchscreenDetected) {
    if (touchscreenDetected)
      return this.i18n('hidDetectionPrerequisitesTouchscreen');
    else
      return this.i18n('hidDetectionPrerequisites');
  },

  /**
   * Provides the label for the mouse row
   */
  getMouseLabel_() {
    var stateToStrMap = new Map([
      [CONNECTION.SEARCHING, 'hidDetectionMouseSearching'],
      [CONNECTION.USB, 'hidDetectionUSBMouseConnected'],
      [CONNECTION.CONNECTED, 'hidDetectionPointingDeviceConnected'],
      [CONNECTION.PAIRING, 'hidDetectionPointingDeviceConnected'],
      [CONNECTION.PAIRED, 'hidDetectionBTMousePaired'],
    ]);

    if (stateToStrMap.has(this.mouseState_))
      return this.i18n(stateToStrMap.get(this.mouseState_));
    else
      return '';
  },

  /**
   * Provides the label for the keyboard row
   */
  getKeyboardLabel_() {
    switch (this.keyboardState_) {
      case CONNECTION.SEARCHING:
        return this.i18n('hidDetectionKeyboardSearching');
      case CONNECTION.USB:
      case CONNECTION.CONNECTED:
        return this.i18n('hidDetectionUSBKeyboardConnected');
      case CONNECTION.PAIRED:
        return this.i18n(
            'hidDetectionBluetoothKeyboardPaired', this.keyboardDeviceName);
      case CONNECTION.PAIRING:
        return this.i18n(
            'hidDetectionKeyboardPairing', this.keyboardDeviceName);
    }
  },

  /**
   * If the user accidentally closed the PIN dialog, tapping on on the keyboard
   * row while the dialog should be visible will reopen it.
   */
  openPinDialog_() {
    this.onPinDialogVisibilityChanged_();
  },

  /**
   * Helper function to calculate visibility of 'connected' icons.
   * @param {string} state Connection state (one of CONNECTION).
   * @private
   */
  tickIsVisible_(state) {
    return (state == CONNECTION.USB) || (state == CONNECTION.CONNECTED) ||
        (state == CONNECTION.PAIRED);
  },

  /**
   * Helper function to calculate visibility of the spinner.
   * @param {string} state Connection state (one of CONNECTION).
   * @private
   */
  spinnerIsVisible_(state) {
    return state == CONNECTION.SEARCHING;
  },

  /**
   * Updates the visibility of the PIN dialog.
   * @private
   */
  onPinDialogVisibilityChanged_() {
    if (this.pinDialogVisible_) {
      if (!this.pinDialogIsOpen_) {
        this.$['hid-pin-popup'].showDialog();
        this.pinDialogIsOpen_ = true;
      }
    } else {
      this.$['hid-pin-popup'].hideDialog();
      this.pinDialogIsOpen_ = false;
    }
  },

  /**
   * Sets the title of the PIN dialog according to the device's name.
   */
  getPinDialogTitle_() {
    return this.i18n('hidDetectionPinDialogTitle', this.keyboardDeviceName);
  },

  /**
   *  Modifies the PIN that is seen on the PIN dialog.
   *  Also marks the current number to be entered with the class 'key-next'.
   */
  onPinParametersChanged_() {
    const keysEntered = this.numKeysEnteredPinCode;
    for (let i = 0; i < PINCODE_LENGTH; i++) {
      const pincodeSymbol = this.$['hid-pincode-sym-' + (i + 1)];
      pincodeSymbol.classList.toggle('key-next', i == keysEntered);
      if (i < PINCODE_LENGTH)
        pincodeSymbol.textContent = this.pinCode[i] ? this.pinCode[i] : '';
    }
  },

  /**
   * Action to be taken when the user closes the PIN dialog before finishing
   * the pairing process.
   */
  onPinDialogClosed_() {
    this.pinDialogIsOpen_ = false;
  },

  /**
   * This is 'on-tap' event handler for 'Continue' button.
   */
  onHIDContinueTap_(event) {
    this.userActed('HIDDetectionOnContinue');
    event.stopPropagation();
  },

  /**
   * Sets TouchscreenDetected to true
   */
  setTouchscreenDetectedState(state) {
    this.touchscreenDetected_ = state;
  },

  /**
   * Sets current state in keyboard pairing process.
   * @param {string} state Connection state (one of CONNECTION).
   */
  setKeyboardState(state) {
    this.keyboardState_ = state;
  },

  /**
   * Sets current state in mouse pairing process.
   * @param {string} state Connection state (one of CONNECTION).
   */
  setMouseState(state) {
    this.mouseState_ = state;
  },

  setKeyboardPinCode(pin) {
    this.pinCode = pin;
  },

  setPinDialogVisible(visibility) {
    this.pinDialogVisible_ = visibility;
  },

  setNumKeysEnteredPinCode(num_keys) {
    this.numKeysEnteredPinCode = num_keys;
  },

  setPointingDeviceName(device_name) {
    this.pointingDeviceName = device_name;
  },

  setKeyboardDeviceName(device_name) {
    this.keyboardDeviceName = device_name;
  },

  setContinueButtonEnabled(enabled) {
    this.continueButtonEnabled = enabled;
  },
});
})();
