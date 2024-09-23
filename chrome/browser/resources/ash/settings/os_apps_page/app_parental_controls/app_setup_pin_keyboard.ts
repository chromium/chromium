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

import {AppParentalControlsHandlerInterface, PinValidationResult} from '../../mojom-webui/app_parental_controls_handler.mojom-webui.js';

import {getTemplate} from './app_setup_pin_keyboard.html.js';
import {ParentalControlsPinDialogError, recordPinDialogError} from './metrics_utils.js';
import {getAppParentalControlsProvider} from './mojo_interface_provider.js';

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

  private initialPin_: string;
  private isSetPinCallPending_: boolean;
  private mojoInterfaceProvider: AppParentalControlsHandlerInterface;
  private pinKeyboardValue_: string;
  private problemMessage_: string;

  constructor() {
    super();
    this.mojoInterfaceProvider = getAppParentalControlsProvider();
  }

  override focus(): void {
    this.$.pinKeyboard.focusInput();
  }

  override connectedCallback(): void {
    this.resetState();
  }

  resetState(): void {
    this.initialPin_ = '';
    this.isConfirmStep = false;
    this.resetPinKeyboard_();
  }

  private resetPinKeyboard_(): void {
    this.pinKeyboardValue_ = '';
    this.problemMessage_ = '';
    this.enableSubmit = false;
  }

  /**
   * Returns true if the user has re-entered the same PIN at the confirmation
   * step.
   */
  private isPinConfirmed_(): boolean {
    return this.isConfirmStep && (this.initialPin_ === this.pinKeyboardValue_);
  }

  private async canGoToConfirmStep_(newPin: string): Promise<boolean> {
    this.problemMessage_ = '';

    if (newPin.length === 0) {
      return false;
    }

    if (this.isConfirmStep) {
      return true;
    }

    // Check if a valid PIN is entered before going to confirm step.
    const validationResult =
        await this.mojoInterfaceProvider.validatePin(newPin);
    switch (validationResult.result) {
      case (PinValidationResult.kPinValidationSuccess):
        return true;
      case (PinValidationResult.kPinLengthError):
        this.problemMessage_ = this.i18n(MessageType.WRONG_LENGTH);
        return false;
      case (PinValidationResult.kPinNumericError):
        this.problemMessage_ = this.i18n(MessageType.NUMBERS_ONLY);
        return false;
    }
  }

  private async onPinChange_(e: CustomEvent<{pin: string}>): Promise<void> {
    const newPin = e.detail.pin;
    this.enableSubmit = await this.canGoToConfirmStep_(newPin);
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
      this.isConfirmStep = true;
      this.resetPinKeyboard_();
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
    this.mojoInterfaceProvider.setUpPin(this.pinKeyboardValue_)
        .then((result) => {
          this.isSetPinCallPending_ = false;
          if (!result.isSuccess) {
            // An error is not expected because the PIN is validated before
            // proceeding to the confirmation step where the PIN is stored.
            console.error('app-controls: Failed to set PIN');
            return;
          }
          this.dispatchEvent(new Event('set-app-pin-done', {composed: true}));
        });
  }

  private hasError_(): boolean {
    return this.problemMessage_ === this.i18n(MessageType.MISMATCH);
  }

  private getErrorClass_(): string {
    return this.hasError_() ? 'error' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppSetupPinKeyboardElement.is]: AppSetupPinKeyboardElement;
  }
}

customElements.define(
    AppSetupPinKeyboardElement.is, AppSetupPinKeyboardElement);
