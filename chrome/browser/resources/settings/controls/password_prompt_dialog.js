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

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '../settings_shared_css.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'settings-password-prompt-dialog',

  _template: html`{__html_template__}`,

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
     * @type {Object}
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
    return /** @type {!CrInputElement} */ (this.$.passwordInput);
  },

  /** @override */
  attached() {
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
  onCancelTap_() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  },

  /**
   * Run the account password check.
   * @private
   */
  submitPassword_() {
    this.waitingForPasswordCheck_ = true;

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

      this.fire('token-obtained', tokenInfo);
      this.passwordInvalid_ = false;

      if (this.$.dialog.open) {
        this.$.dialog.close();
      }
    });
  },

  /** @private */
  onInputValueChange_() {
    this.passwordInvalid_ = false;
  },

  /** @private */
  isConfirmEnabled_() {
    return !this.waitingForPasswordCheck_ && !this.passwordInvalid_ &&
        this.inputValue_;
  },
});
