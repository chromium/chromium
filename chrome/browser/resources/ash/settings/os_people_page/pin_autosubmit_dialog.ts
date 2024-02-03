// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'pin-autosubmit-confirmation-dialog' is a confirmation dialog that pops up
 * when the user chooses to enable PIN auto submit. The user is prompted to
 * enter their current PIN and if it matches the feature is enabled.
 *
 */

import 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import 'chrome://resources/ash/common/quick_unlock/setup_pin_keyboard.js';
import 'chrome://resources/js/assert.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';

import {PinKeyboardElement} from 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import {fireAuthTokenInvalidEvent} from 'chrome://resources/ash/common/quick_unlock/utils.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './pin_autosubmit_dialog.html.js';

// Maximum length supported by auto submit
const AutosubmitMaxLength = 12;

// Possible errors that might occur with the respective i18n string.
const AutoSubmitErrorStringsName = {
  PinIncorrect: 'pinAutoSubmitPinIncorrect',
  PinTooLong: 'pinAutoSubmitLongPinError',
};

const SettingsPinAutosubmitDialogElementBase = I18nMixin(PolymerElement);

interface SettingsPinAutosubmitDialogElement {
  $: {
    dialog: CrDialogElement,
    pinKeyboard: PinKeyboardElement,
  };
}

class SettingsPinAutosubmitDialogElement extends
    SettingsPinAutosubmitDialogElementBase {
  static get is() {
    return 'settings-pin-autosubmit-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current PIN keyboard value.
       */
      pinValue_: {
        type: String,
      },

      /**
       * Possible errors that might occur. Null when there are no errors to
       * show.
       */
      error_: {
        type: String,
        value: null,
      },

      /**
       * Whether there is a request in process already. Disables the
       * buttons, but leaves the cancel button actionable.
       */
      requestInProcess_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the confirm button should be disabled.
       */
      confirmButtonDisabled_: {
        type: Boolean,
        value: true,
      },

      /**
       * Authentication token provided by lock-screen-password-prompt-dialog.
       */
      authToken: {
        type: String,
        notify: true,
      },

      /**
       * Interface for chrome.quickUnlockPrivate calls. May be overridden by
       * tests.
       */
      quickUnlockPrivate: {type: Object, value: chrome.quickUnlockPrivate},
    };
  }

  authToken: string|undefined;
  private error_: string|null;
  private confirmButtonDisabled_: boolean;
  private pinValue_: string;
  private quickUnlockPrivate: typeof chrome.quickUnlockPrivate;
  private requestInProcess_: boolean;

  static get observers() {
    return [
      'updateButtonState_(error_, requestInProcess_, pinValue_)',
    ];
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.resetState();
    this.$.dialog.showModal();
    this.$.pinKeyboard.focusInput();
  }

  close(): void {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
    this.resetState();
  }

  resetState(): void {
    this.requestInProcess_ = false;
    this.pinValue_ = '';
    this.error_ = null;
  }

  private onCancelClick_(): void {
    this.close();
  }

  /**
   * Update error notice when more digits are inserted.
   * e: Custom event containing the new pin
   */
  private onPinChange_(e: CustomEvent<{pin: string}>): void {
    if (e && e.detail && e.detail.pin) {
      this.pinValue_ = e.detail.pin;
    }

    if (this.pinValue_ && this.pinValue_.length > AutosubmitMaxLength) {
      this.error_ = AutoSubmitErrorStringsName['PinTooLong'];
      return;
    }

    this.error_ = null;
  }

  /**
   * Make a request to the quick unlock API to enable PIN auto-submit.
   */
  private onPinSubmit_(): void {
    // Prevent submission through 'ENTER' if the 'Submit' button is disabled
    this.updateButtonState_();
    if (this.confirmButtonDisabled_) {
      return;
    }

    if (typeof this.authToken !== 'string') {
      fireAuthTokenInvalidEvent(this);
      return;
    }

    // Make a request to enable pin autosubmit.
    this.requestInProcess_ = true;

    this.quickUnlockPrivate.setPinAutosubmitEnabled(
        this.authToken, this.pinValue_ /* PIN */, true /*enabled*/,
        this.onPinSubmitResponse_.bind(this));
  }

  /**
   * Response from the quick unlock API.
   *
   * If the call is not successful because the PIN is incorrect,
   * it will check if its still possible to authenticate with PIN.
   * Submitting an invalid PIN will either show an error to the user,
   * or close the dialog and trigger a password re-prompt.
   */
  private onPinSubmitResponse_(success: boolean): void {
    if (success) {
      this.close();
      return;
    }
    // Check if it is still possible to authenticate with pin.
    this.quickUnlockPrivate.canAuthenticatePin(
        this.onCanAuthenticateResponse_.bind(this));
  }

  /**
   * Response from the quick unlock API on whether PIN authentication
   * is currently possible.
   */
  private onCanAuthenticateResponse_(canAuthenticate: boolean): void {
    if (!canAuthenticate) {
      const event = new CustomEvent(
          'invalidate-auth-token-requested', {bubbles: true, composed: true});
      this.dispatchEvent(event);
      this.close();
      return;
    }
    // The entered PIN was incorrect.
    this.pinValue_ = '';
    this.requestInProcess_ = false;
    this.error_ = AutoSubmitErrorStringsName.PinIncorrect;
    this.$.pinKeyboard.focusInput();
  }

  private updateButtonState_(): void {
    this.confirmButtonDisabled_ =
        this.requestInProcess_ || !!this.error_ || !this.pinValue_;
  }

  /**
   * Error message to be shown on the dialog when the PIN is
   * incorrect, or if its too long to activate auto submit.
   * error: i18n String
   */
  private getErrorMessageString_(error: string|null): string {
    return error ? this.i18n(error) : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPinAutosubmitDialogElement.is]: SettingsPinAutosubmitDialogElement;
  }
}

customElements.define(
    SettingsPinAutosubmitDialogElement.is, SettingsPinAutosubmitDialogElement);
