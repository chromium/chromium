// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design HID detection
 * screen.
 */

import '//resources/ash/common/bluetooth/bluetooth_pairing_enter_code_page.js';
import '../../components/hd_iron_icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_modal_dialog.js';
import '../../components/buttons/oobe_text_button.js';

import {IronA11yAnnouncer} from '//resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './hid_detection.html.js';

/**
 * Enumeration of possible connection states of a device.
 */
enum Connection {
  SEARCHING = 'searching',
  USB = 'usb',
  CONNECTED = 'connected',
  PAIRING = 'pairing',
  PAIRED = 'paired',
}

const HidDetectionScreenBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class HidDetectionScreen extends HidDetectionScreenBase {
  static get is() {
    return 'hid-detection-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * "Continue" button is disabled until HID devices are paired.
       */
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
       */
      touchscreenDetected_: {
        type: Boolean,
        value: false,
      },

      /**
       * Current state in mouse pairing process.
       */
      mouseState: {
        type: String,
        value: Connection.SEARCHING,
      },

      /**
       * Current state in keyboard pairing process.
       */
      keyboardState: {
        type: String,
        value: Connection.SEARCHING,
      },

      /**
       * Controls the visibility of the PIN dialog.
       */
      pinDialogVisible: {
        type: Boolean,
        value: false,
        observer: 'onPinDialogVisibilityChanged',
      },

      /**
       * The PIN code to be typed by the user
       */
      pinCode: {
        type: String,
        value: '000000',
      },

      /**
       * The number of keys that the user already entered for this PIN.
       * This helps the user to see what's the next key to be pressed.
       */
      numKeysEnteredPinCode: {
        type: Number,
        value: 0,
      },

      /**
       *  Whether the dialog for PIN input is being shown.
       *  Internal use only. Used for preventing multiple openings.
       */
      pinDialogIsOpen: {
        type: Boolean,
        value: false,
      },

      /**
       * The title that is displayed on the PIN dialog
       */
      pinDialogTitle: {
        type: String,
        computed: 'getPinDialogTitle(locale, keyboardDeviceName)',
      },
    };
  }

  continueButtonEnabled: boolean;
  keyboardDeviceName: string;
  pointingDeviceName: string;
  private touchscreenDetected_: boolean;
  private mouseState: Connection;
  private keyboardState: Connection;
  private pinDialogVisible: boolean;
  pinCode: string;
  numKeysEnteredPinCode: number;
  private pinDialogIsOpen: boolean;
  pinDialogTitle: string;

  override get EXTERNAL_API(): string[] {
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

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('HIDDetectionScreen');
    IronA11yAnnouncer.requestAvailability();
  }

  private getPrerequisitesText(locale: string, touchscreenDetected: boolean) {
    if (touchscreenDetected) {
      return this.i18nDynamic(locale, 'hidDetectionPrerequisitesTouchscreen');
    } else {
      return this.i18nDynamic(locale, 'hidDetectionPrerequisites');
    }
  }

  /**
   * Provides the label for the mouse row
   */
  private getMouseLabel(): string {
    const stateToStrMap = new Map([
      [Connection.SEARCHING, 'hidDetectionMouseSearching'],
      [Connection.USB, 'hidDetectionUSBMouseConnected'],
      [Connection.CONNECTED, 'hidDetectionPointingDeviceConnected'],
      [Connection.PAIRING, 'hidDetectionPointingDeviceConnected'],
      [Connection.PAIRED, 'hidDetectionBTMousePaired'],
    ]);

    if (stateToStrMap.has(this.mouseState)) {
      return this.i18n(stateToStrMap.get(this.mouseState)!);
    } else {
      return '';
    }
  }

  /**
   * Provides the label for the keyboard row
   */
  private getKeyboardLabel(): string {
    switch (this.keyboardState) {
      case Connection.SEARCHING:
        return this.i18n('hidDetectionKeyboardSearching');
      case Connection.USB:
      case Connection.CONNECTED:
        return this.i18n('hidDetectionUSBKeyboardConnected');
      case Connection.PAIRED:
        return this.i18n(
            'hidDetectionBluetoothKeyboardPaired', this.keyboardDeviceName);
      case Connection.PAIRING:
        return this.i18n(
            'hidDetectionKeyboardPairing', this.keyboardDeviceName);
    }
  }

  /**
   * If the user accidentally closed the PIN dialog, tapping on on the keyboard
   * row while the dialog should be visible will reopen it.
   */
  private openPinDialog(): void {
    this.onPinDialogVisibilityChanged();
  }

  /**
   * Helper function to calculate visibility of 'connected' icons.
   * @param state Connection state (one of Connection).
   */
  private tickIsVisible(state: Connection): boolean {
    return (state === Connection.USB) || (state === Connection.CONNECTED) ||
        (state === Connection.PAIRED);
  }

  /**
   * Helper function to calculate visibility of the spinner.
   * @param state Connection state (one of Connection).
   */
  private spinnerIsVisible(state: Connection): boolean {
    return state === Connection.SEARCHING;
  }

  /**
   * Updates the visibility of the PIN dialog.
   */
  private onPinDialogVisibilityChanged(): void {
    const dialog =
        this.shadowRoot?.querySelector<OobeModalDialog>('#hid-pin-popup');

    // Return early if element is not yet attached to the page.
    if (!dialog) {
      return;
    }

    if (this.pinDialogVisible) {
      if (!this.pinDialogIsOpen) {
        dialog.showDialog();
        this.pinDialogIsOpen = true;
      }
    } else {
      dialog.hideDialog();
      this.pinDialogIsOpen = false;
    }
  }

  /**
   * Sets the title of the PIN dialog according to the device's name.
   */
  private getPinDialogTitle(): string {
    return this.i18n('hidDetectionPinDialogTitle', this.keyboardDeviceName);
  }

  /**
   * Action to be taken when the user closes the PIN dialog before finishing
   * the pairing process.
   */
  private onPinDialogClosed(): void {
    this.pinDialogIsOpen = false;
  }

  /**
   * Action to be taken when the user closes the PIN dialog before finishing
   * the pairing process.
   */
  private onCancel(event: Event): void {
    event.stopPropagation();
    const hidPinPopupDialog = this.shadowRoot?.querySelector('#hid-pin-popup');
    if (hidPinPopupDialog instanceof OobeModalDialog) {
      hidPinPopupDialog.hideDialog();
    }
  }

  /**
   * This is 'on-tap' event handler for 'Continue' button.
   */
  private onHidContinueClick(event: Event): void {
    this.userActed('HIDDetectionOnContinue');
    event.stopPropagation();
  }

  /**
   * Sets TouchscreenDetected to true
   */
  setTouchscreenDetectedState(state: boolean): void {
    this.touchscreenDetected_ = state;
  }

  /**
   * Sets current state in keyboard pairing process.
   * @param state Connection state (one of Connection).
   */
  setKeyboardState(state: Connection): void {
    this.keyboardState = state;
  }

  /**
   * Sets current state in mouse pairing process.
   * @param state Connection state (one of Connection).
   */
  setMouseState(state: Connection): void {
    this.mouseState = state;
  }

  setKeyboardPinCode(pin: string): void {
    this.pinCode = pin;
  }

  setPinDialogVisible(visibility: boolean): void {
    this.pinDialogVisible = visibility;
  }

  setNumKeysEnteredPinCode(numKeys: number): void {
    this.numKeysEnteredPinCode = numKeys;
  }

  setPointingDeviceName(deviceName: string): void {
    this.pointingDeviceName = deviceName;
  }

  setKeyboardDeviceName(deviceName: string): void {
    this.keyboardDeviceName = deviceName;
  }

  setContinueButtonEnabled(enabled: boolean): void {
    const oldContinueButtonEnabled = this.continueButtonEnabled;
    this.continueButtonEnabled = enabled;
    afterNextRender(this, () => {
      const hidContinueButton =
          this.shadowRoot?.querySelector('#hid-continue-button');
      if (hidContinueButton instanceof HTMLElement) {
        hidContinueButton.focus();
      }
    });
    if (oldContinueButtonEnabled !== enabled) {
      this.announceContinueButtonUpdates();
    }
  }

  private announceContinueButtonUpdates(): void {
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

declare global {
  interface HTMLElementTagNameMap {
    [HidDetectionScreen.is]: HidDetectionScreen;
  }
}

customElements.define(HidDetectionScreen.is, HidDetectionScreen);
