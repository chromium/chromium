// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-setup-pin-keyboard' is the keyboard/input field for
 * entering a PIN to access App Parental Controls. Used by the PIN setup
 * dialog.
 */

import 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PinKeyboardElement} from 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_setup_pin_keyboard.html.js';
import {ParentalControlsPinDialogError, recordPinDialogError} from './metrics_utils.js';

/**
 * Error type mapped to the corresponding i18n string.
 */
enum MessageType {
  WRONG_LENGTH = 'appParentalControlsPinWrongLengthErrorText',
  MISMATCH = 'appParentalControlsPinMismatchErrorText',
  NUMBERS_ONLY = 'appParentalControlsPinNumericErrorText',
}

export const PIN_LENGTH = 6;

const AppSetupPinKeyboardElementBase = (PrefsMixin(I18nMixin(PolymerElement)));

export interface AppSetupPinKeyboardElement {
  $: {
    pinKeyboard: PinKeyboardElement,
  };
}

export class AppSetupPinKeyboardElement extends AppSetupPinKeyboardElementBase {
  static get is() {
    return 'app-setup-pin-keyboard' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether clicking the submit button takes you to the next step.
       */
      enableSubmit: {
        notify: true,
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user is at the PIN confirmation step.
       */
      isConfirmStep: {
        notify: true,
        type: Boolean,
        value: false,
      },

      ariaLabel: {
        type: String,
      },

      /**
       * The current PIN keyboard value.
       */
      pinKeyboardValue_: {
        type: String,
        value: '',
      },

      /**
       * Stores the initial PIN value so it can be confirmed.
       */
      initialPin_: {
        type: String,
        value: '',
      },

      /**
       * Whether the PIN is currently being set as a pref.
       * If true, the PIN keyboard input should be disabled.
       */
      isSetPinCallPending_: {
        notify: true,
        type: Boolean,
        value: false,
      },
    };
  }

  enableSubmit: boolean;
  isConfirmStep: boolean;

  private pinKeyboardValue_: string;
  private initialPin_: string;
  private problemMessage_: string;
  private isSetPinCallPending_: boolean;

  override focus(): void {
    this.$.pinKeyboard.focusInput();
  }

  override connectedCallback(): void {
    this.resetState();
  }

  resetState(): void {
    this.initialPin_ = '';
    this.pinKeyboardValue_ = '';
    this.isConfirmStep = false;
    this.enableSubmit = false;
  }

  /**
   * Returns true if the user has re-entered the same PIN at the confirmation
   * step.
   */
  private isPinConfirmed_(): boolean {
    return this.isConfirmStep && (this.initialPin_ === this.pinKeyboardValue_);
  }

  private canGoToConfirmStep_(newPin: string): boolean {
    this.problemMessage_ = '';

    if (this.isConfirmStep) {
      return true;
    }

    return this.isValidPin_(newPin);
  }

  private isValidPin_(newPin: string): boolean {
    // If no PIN is entered erase the problem message.
    if (newPin.length === 0) {
      this.problemMessage_ = '';
      return true;
    }

    if (newPin.length !== PIN_LENGTH) {
      this.problemMessage_ = this.i18n(MessageType.WRONG_LENGTH);
      return false;
    }

    if (!this.isNumeric_(newPin)) {
      this.problemMessage_ = this.i18n(MessageType.NUMBERS_ONLY);
      return false;
    }

    return true;
  }

  private isNumeric_(str: string): boolean {
    // RegExp to test if all characters are digits.
    return /^\d+$/.test(str);
  }

  private onPinChange_(e: CustomEvent<{pin: string}>): void {
    const newPin = e.detail.pin;
    this.enableSubmit = this.canGoToConfirmStep_(newPin);
  }

  /**
   * Called when the user presses enter/return during PIN entry.
   */
  private onPinSubmit_(): void {
    this.doSubmit();
  }

  /**
   * Intended to be called by the containing dialog when the user attempts
   * to submit the PIN.
   */
  doSubmit(): void {
    if (!this.isConfirmStep) {
      if (!this.enableSubmit) {
        recordPinDialogError(
            ParentalControlsPinDialogError.INVALID_PIN_ON_SETUP);
        return;
      }
      this.initialPin_ = this.pinKeyboardValue_;
      this.pinKeyboardValue_ = '';
      this.isConfirmStep = true;
      this.problemMessage_ = '';
      this.$.pinKeyboard.focusInput();
      return;
    }

    if (!this.isPinConfirmed_()) {
      // Focus the PIN keyboard and highlight the entire PIN so the user can
      // replace it.
      this.$.pinKeyboard.focusInput(0, this.pinKeyboardValue_.length + 1);
      this.problemMessage_ = this.i18n(MessageType.MISMATCH);
      return;
    }

    this.isSetPinCallPending_ = true;
    this.setPrefValue('on_device_app_controls.pin', this.pinKeyboardValue_);
    this.setPrefValue('on_device_app_controls.setup_completed', true);
    this.isSetPinCallPending_ = false;

    this.dispatchEvent(new Event('set-app-pin-done', {composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppSetupPinKeyboardElement.is]: AppSetupPinKeyboardElement;
  }
}

customElements.define(
    AppSetupPinKeyboardElement.is, AppSetupPinKeyboardElement);
