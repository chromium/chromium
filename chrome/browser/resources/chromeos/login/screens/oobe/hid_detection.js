// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design HID detection
 * screen.
 */

import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/ash/common/bluetooth/bluetooth_pairing_enter_code_page.js';
import '../../components/hd_iron_icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_modal_dialog.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {IronA11yAnnouncer} from '//resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';


/** @const {number} */ const PINCODE_LENGTH = 6;

// Enumeration of possible connection states of a device.
const CONNECTION = {
  SEARCHING: 'searching',
  USB: 'usb',
  CONNECTED: 'connected',
  PAIRING: 'pairing',
  PAIRED: 'paired',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const HidDetectionScreenBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

/** @polymer */
class HidDetectionScreen extends HidDetectionScreenBase {
  static get is() {
    return 'hid-detection-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * "Continue" button is disabled until HID devices are paired.
       * @type {boolean}
       */
      continueButtonEnabled: {
        type: Boolean,
        value: false,
      },

      /**
       * The keyboard device name
       * @type {string}
       */
      keyboardDeviceName: {
        type: String,
        value: '',
      },

      /**
       * The pointing device name
       * @type {string}
       */
      pointingDeviceName: {
        type: String,
        value: '',
      },

      /**
       * State of touchscreen detection
       * @private {boolean}
       */
      touchscreenDetected_: {
        type: Boolean,
        value: false,
      },

      /**
       * Current state in mouse pairing process.
       * @private {string}
       */
      mouseState_: {
        type: String,
        value: CONNECTION.SEARCHING,
      },

      /**
       * Current state in keyboard pairing process.
       * @private {string}
       */
      keyboardState_: {
        type: String,
        value: CONNECTION.SEARCHING,
      },

      /**
       * Controls the visibility of the PIN dialog.
       * @private {boolean}
       */
      pinDialogVisible_: {
        type: Boolean,
        value: false,
        observer: 'onPinDialogVisibilityChanged_',
      },

      /**
       * The PIN code to be typed by the user
       * @type {string}
       */
      pinCode: {
        type: String,
        value: '000000',
        observer: 'onPinParametersChanged_',
      },

      /**
       * The number of keys that the user already entered for this PIN.
       * This helps the user to see what's the next key to be pressed.
       * @type {number}
       */
      numKeysEnteredPinCode: {
        type: Number,
        value: 0,
        observer: 'onPinParametersChanged_',
      },

      /**
       *  Whether the dialog for PIN input is being shown.
       *  Internal use only. Used for preventing multiple openings.
       * @private {boolean}
       */
      pinDialogIsOpen_: {
        type: Boolean,
        value: false,
      },

      /**
       * The title that is displayed on the PIN dialog
       * @type {string}
       */
      pinDialogTitle: {
        type: String,
        computed: 'getPinDialogTitle_(locale, keyboardDeviceName)',
      },

      /**
       * True when kOobeHidDetectionRevamp is enabled.
       * @private
       * @type {boolean}
       */
      isOobeHidDetectionRevampEnabled_: {
        type: Boolean,
        value: loadTimeData.getBoolean('enableOobeHidDetectionRevamp'),
      },
    };
  }

  get EXTERNAL_API() {
    return [
      'setKeyboardState',
      'setMouseState',
      'setKeyboardPinCode',
      'setPinDialogVisible',
      'setNumKeysEnteredPinCode',
      'setPointingDeviceName',
      'setKeyboardDeviceName',
      'setTouchscreenDetectedState',
      'setContinueButtonEnabled',
    ];
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('HIDDetectionScreen');
    IronA11yAnnouncer.requestAvailability();
  }

  getPrerequisitesText_(locale, touchscreenDetected) {
    if (touchscreenDetected) {
      return this.i18n('hidDetectionPrerequisitesTouchscreen');
    } else {
      return this.i18n('hidDetectionPrerequisites');
    }
  }

  /**
   * Provides the label for the mouse row
   */
  getMouseLabel_() {
    const stateToStrMap = new Map([
      [CONNECTION.SEARCHING, 'hidDetectionMouseSearching'],
      [CONNECTION.USB, 'hidDetectionUSBMouseConnected'],
      [CONNECTION.CONNECTED, 'hidDetectionPointingDeviceConnected'],
      [CONNECTION.PAIRING, 'hidDetectionPointingDeviceConnected'],
      [CONNECTION.PAIRED, 'hidDetectionBTMousePaired'],
    ]);

    if (stateToStrMap.has(this.mouseState_)) {
      return this.i18n(stateToStrMap.get(this.mouseState_));
    } else {
      return '';
    }
  }

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
  }

  /**
   * If the user accidentally closed the PIN dialog, tapping on on the keyboard
   * row while the dialog should be visible will reopen it.
   */
  openPinDialog_() {
    this.onPinDialogVisibilityChanged_();
  }

  /**
   * Helper function to calculate visibility of 'connected' icons.
   * @param {string} state Connection state (one of CONNECTION).
   * @private
   */
  tickIsVisible_(state) {
    return (state == CONNECTION.USB) || (state == CONNECTION.CONNECTED) ||
        (state == CONNECTION.PAIRED);
  }

  /**
   * Helper function to calculate visibility of the spinner.
   * @param {string} state Connection state (one of CONNECTION).
   * @private
   */
  spinnerIsVisible_(state) {
    return state == CONNECTION.SEARCHING;
  }

  /**
   * Updates the visibility of the PIN dialog.
   * @private
   */
  onPinDialogVisibilityChanged_() {
    const dialog = this.shadowRoot.querySelector('#hid-pin-popup');

    // Return early if element is not yet attached to the page.
    if (!dialog) {
      return;
    }

    if (this.pinDialogVisible_) {
      if (!this.pinDialogIsOpen_) {
        dialog.showDialog();
        this.pinDialogIsOpen_ = true;
        this.onPinParametersChanged_();
      }
    } else {
      dialog.hideDialog();
      this.pinDialogIsOpen_ = false;
    }
  }

  /**
   * Sets the title of the PIN dialog according to the device's name.
   */
  getPinDialogTitle_() {
    return this.i18n('hidDetectionPinDialogTitle', this.keyboardDeviceName);
  }

  /**
   *  Modifies the PIN that is seen on the PIN dialog.
   *  Also marks the current number to be entered with the class 'key-next'.
   */
  onPinParametersChanged_() {
    if (this.isOobeHidDetectionRevampEnabled_ || !this.pinDialogVisible_) {
      return;
    }

    const keysEntered = this.numKeysEnteredPinCode;
    for (let i = 0; i < PINCODE_LENGTH; i++) {
      const pincodeSymbol =
          this.shadowRoot.querySelector('#hid-pincode-sym-' + (i + 1));
      pincodeSymbol.classList.toggle('key-next', i == keysEntered);
      if (i < PINCODE_LENGTH) {
        pincodeSymbol.textContent = this.pinCode[i] ? this.pinCode[i] : '';
      }
    }
  }

  /**
   * Action to be taken when the user closes the PIN dialog before finishing
   * the pairing process.
   */
  onPinDialogClosed_() {
    this.pinDialogIsOpen_ = false;
  }

  /**
   * Action to be taken when the user closes the PIN dialog before finishing
   * the pairing process.
   * @param {!Event} event
   * @private
   */
  onCancel_(event) {
    event.stopPropagation();
    this.shadowRoot.querySelector('#hid-pin-popup').hideDialog();
  }

  /**
   * This is 'on-tap' event handler for 'Continue' button.
   */
  onHIDContinueTap_(event) {
    this.userActed('HIDDetectionOnContinue');
    event.stopPropagation();
  }

  /**
   * Sets TouchscreenDetected to true
   */
  setTouchscreenDetectedState(state) {
    this.touchscreenDetected_ = state;
  }

  /**
   * Sets current state in keyboard pairing process.
   * @param {string} state Connection state (one of CONNECTION).
   */
  setKeyboardState(state) {
    this.keyboardState_ = state;
  }

  /**
   * Sets current state in mouse pairing process.
   * @param {string} state Connection state (one of CONNECTION).
   */
  setMouseState(state) {
    this.mouseState_ = state;
  }

  setKeyboardPinCode(pin) {
    this.pinCode = pin;
  }

  setPinDialogVisible(visibility) {
    this.pinDialogVisible_ = visibility;
  }

  setNumKeysEnteredPinCode(num_keys) {
    this.numKeysEnteredPinCode = num_keys;
  }

  setPointingDeviceName(device_name) {
    this.pointingDeviceName = device_name;
  }

  setKeyboardDeviceName(device_name) {
    this.keyboardDeviceName = device_name;
  }

  setContinueButtonEnabled(enabled) {
    const oldContinueButtonEnabled = this.continueButtonEnabled;
    this.continueButtonEnabled = enabled;
    afterNextRender(this, () => this.$['hid-continue-button'].focus());

    if (oldContinueButtonEnabled != enabled) {
      this.announceContinueButtonUpdates_();
    }
  }

  /** @protected */
  announceContinueButtonUpdates_() {
    this.dispatchEvent(new CustomEvent('iron-announce', {
      bubbles: true,
      composed: true,
      detail: {
        text: this.continueButtonEnabled ?
            this.i18n('hidDetectionA11yContinueEnabled') :
            this.i18n('hidDetectionA11yContinueDisabled'),
      },
    }));
  }
}

customElements.define(HidDetectionScreen.is, HidDetectionScreen);
