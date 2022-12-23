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

import 'chrome://resources/ash/common/quick_unlock/setup_pin_keyboard.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/assert.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './pin_autosubmit_dialog.html.js';

// Maximum length supported by auto submit
const AutosubmitMaxLength = 12;

// Possible errors that might occur with the respective i18n string.
const AutoSubmitErrorStringsName = {
  PinIncorrect: 'pinAutoSubmitPinIncorrect',
  PinTooLong: 'pinAutoSubmitLongPinError',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsPinAutosubmitDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsPinAutosubmitDialogElement extends
    SettingsPinAutosubmitDialogElementBase {
  static get is() {
    return 'settings-pin-autosubmit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current PIN keyboard value.
       * @private
       */
      pinValue_: {
        type: String,
      },

      /**
       * Possible errors that might occur. Null when there are no errors to
       * show.
       * @private {?string}
       */
      error_: {
        type: String,
        value: null,
      },

      /**
       * Whether there is a request in process already. Disables the
       * buttons, but leaves the cancel button actionable.
       * @private
       */
      requestInProcess_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the confirm button should be disabled.
       * @private
       */
      confirmButtonDisabled_: {
        type: Boolean,
        value: true,
      },

      /**
       * Authentication token provided by lock-screen-password-prompt-dialog.
       * @type {!chrome.quickUnlockPrivate.TokenInfo|undefined}
       */
      authToken: {
        type: Object,
        notify: true,
      },

      /**
       * Interface for chrome.quickUnlockPrivate calls. May be overridden by
       * tests.
       * @private
       */
      quickUnlockPrivate: {type: Object, value: chrome.quickUnlockPrivate},
    };
  }

  static get observers() {
    return [
      'updateButtonState_(error_, requestInProcess_, pinValue_)',
    ];
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.resetState();
    this.$.dialog.showModal();
    this.$.pinKeyboard.focusInput();
  }

  close() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
    this.resetState();
  }

  resetState() {
    this.requestInProcess_ = false;
    this.pinValue_ = '';
    this.error_ = null;
  }

  /** @private */
  onCancelTap_() {
    this.close();
  }

  /**
   * Update error notice when more digits are inserted.
   * @param {!CustomEvent<{pin: string}>} e Custom event containing the new pin
   * @private
   */
  onPinChange_(e) {
    if (e && e.detail && e.detail.pin) {
      this.pinValue_ = e.detail.pin;
    }

    if (this.pinValue_ && this.pinValue_.length > AutosubmitMaxLength) {
      this.error_ = AutoSubmitErrorStringsName.PinTooLong;
      return;
    }

    this.error_ = null;
  }

  /**
   * Make a request to the quick unlock API to enable PIN auto-submit.
   * @private
   */
  onPinSubmit_() {
    // Prevent submission through 'ENTER' if the 'Submit' button is disabled
    this.updateButtonState_();
    if (this.confirmButtonDisabled_) {
      return;
    }

    // Make a request to enable pin autosubmit.
    this.requestInProcess_ = true;
    this.quickUnlockPrivate.setPinAutosubmitEnabled(
        this.authToken.token, this.pinValue_ /* PIN */, true /*enabled*/,
        this.onPinSubmitResponse_.bind(this));
  }

  /**
   * Response from the quick unlock API.
   *
   * If the call is not successful because the PIN is incorrect,
   * it will check if its still possible to authenticate with PIN.
   * Submitting an invalid PIN will either show an error to the user,
   * or close the dialog and trigger a password re-prompt.
   * @param {Boolean} success
   * @private
   */
  onPinSubmitResponse_(success) {
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
   * @param {Boolean} can_authenticate
   */
  onCanAuthenticateResponse_(can_authenticate) {
    if (!can_authenticate) {
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

  /** @private */
  updateButtonState_() {
    this.confirmButtonDisabled_ =
        this.requestInProcess_ || !!this.error_ || !this.pinValue_;
  }

  /**
   * Error message to be shown on the dialog when the PIN is
   * incorrect, or if its too long to activate auto submit.
   * @param {?String} error - i18n String
   * @private
   */
  getErrorMessageString_(error) {
    return error ? this.i18n(error) : '';
  }
}

customElements.define(
    SettingsPinAutosubmitDialogElement.is, SettingsPinAutosubmitDialogElement);
