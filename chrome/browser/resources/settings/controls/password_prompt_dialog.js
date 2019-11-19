// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-password-prompt-dialog' shows a dialog which asks for the user to
 * enter their password. It validates the password is correct. Once the user has
 * entered their account password, the page fires an 'authenticated' event and
 * updates the authToken binding.
 *
 * Example:
 *
 * <settings-password-prompt-dialog
 *   id="passwordPrompt"
 *   password-prompt-text="{{passwordPromptText}}"
 *   auth-token="{{authToken}}">
 * </settings-password-prompt-dialog>
 */

(function() {
'use strict';

Polymer({
  is: 'settings-password-prompt-dialog',

  properties: {
    /**
     * The subtext to be displayed above the password input field. Embedders
     * may choose to change this value for their specific use case.
     * @type {string}
     */
    passwordPromptText: {
      type: String,
      notify: true,
      value: '',
    },

    /**
     * Authentication token returned by quickUnlockPrivate.getAuthToken().
     * Should be passed to API calls which require authentication.
     * @type {string}
     */
    authToken: {
      type: String,
      notify: true,
    },

    /**
     * @private {string}
     */
    inputValue_: {
      type: String,
      value: '',
      observer: 'onInputValueChange_',
    },

    /**
     * Helper property which marks password as valid/invalid.
     * @private {boolean}
     */
    passwordInvalid_: {
      type: Boolean,
      value: false,
    },

    /**
     * Interface for chrome.quickUnlockPrivate calls. May be overridden by
     * tests.
     * @type {QuickUnlockPrivate}
     */
    quickUnlockPrivate: {type: Object, value: chrome.quickUnlockPrivate},

    /** @private {boolean} */
    waitingForPasswordCheck_: {
      type: Boolean,
      value: false,
    },
  },

  /** @return {!CrInputElement} */
  get passwordInput() {
    return this.$.passwordInput;
  },

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
    // This needs to occur at the next paint otherwise the password input will
    // not receive focus.
    this.async(() => {
      // TODO(crbug.com/876377): This is unusual; the 'autofocus' attribute on
      // the cr-input element should work. Investigate.
      this.passwordInput.focus();
    }, 1 /* waitTime */);
  },

  /** @private */
  onCancelTap_: function() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  },

  /**
   * The timeout ID to pass to clearTimeout() to cancel auth token
   * invalidation.
   * @private {number|undefined}
   */
  clearAccountPasswordTimeoutId_: undefined,

  /**
   * Run the account password check.
   * @private
   */
  submitPassword_: function() {
    this.waitingForPasswordCheck_ = true;
    clearTimeout(this.clearAccountPasswordTimeoutId_);

    const password = this.passwordInput.value;
    // The user might have started entering a password and then deleted it all.
    // Do not submit/show an error in this case.
    if (!password) {
      this.passwordInvalid_ = false;
      this.waitingForPasswordCheck_ = false;
      return;
    }

    this.quickUnlockPrivate.getAuthToken(password, (tokenInfo) => {
      this.waitingForPasswordCheck_ = false;
      if (chrome.runtime.lastError) {
        this.passwordInvalid_ = true;
        // Select the whole password if user entered an incorrect password.
        this.passwordInput.select();
        return;
      }

      this.authToken = tokenInfo.token;
      this.passwordInvalid_ = false;

      // Clear |this.authToken| after tokenInfo.lifetimeSeconds.
      // Subtract time from the expiration time to account for IPC delays.
      // Treat values less than the minimum as 0 for testing.
      const IPC_SECONDS = 2;
      const lifetimeMs = tokenInfo.lifetimeSeconds > IPC_SECONDS ?
          (tokenInfo.lifetimeSeconds - IPC_SECONDS) * 1000 :
          0;
      this.clearAccountPasswordTimeoutId_ = setTimeout(() => {
        this.authToken = '';
      }, lifetimeMs);

      if (this.$.dialog.open) {
        this.$.dialog.close();
      }
    });
  },

  /** @private */
  onInputValueChange_: function() {
    this.passwordInvalid_ = false;
  },

  /** @private */
  isConfirmEnabled_: function() {
    return !this.waitingForPasswordCheck_ && !this.passwordInvalid_ &&
        this.inputValue_;
  },
});
})();
