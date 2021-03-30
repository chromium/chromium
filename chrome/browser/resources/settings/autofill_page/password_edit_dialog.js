// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-edit-dialog' is the dialog that allows showing a
 *     saved password.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../icons.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';
import './passwords_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
import {ShowPasswordBehavior} from './show_password_behavior.js';

Polymer({
  is: 'password-edit-dialog',

  _template: html`{__html_template__}`,

  behaviors: [ShowPasswordBehavior, I18nBehavior],

  properties: {
    shouldShowStorageDetails: {type: Boolean, value: false},

    /**
     * Saved passwords after deduplicating versions that are repeated in the
     * account and on the device.
     * @type {!Array<!MultiStorePasswordUiEntry>}
     */
    savedPasswords: {
      type: Array,
      value: () => [],
    },

    /**
     * Usernames for the same website. Used for the fast check whether edited
     * username is already used.
     * @private {?Set<string>}
     */
    usernamesForSameOrigin: {
      type: Object,
      value: null,
    },

    /**
     * Check if entry isn't federation credential.
     * @private
     */
    isEditDialog_: {
      type: Boolean,
      computed: 'computeIsEditDialog_(entry)',
    },

    /**
     * Whether the password is visible or obfuscated.
     * @private
     */
    isPasswordVisible_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the username input is invalid.
     * @private
     */
    usernameInputInvalid_: Boolean,

    /**
     * Whether the password input is invalid.
     * @private
     */
    passwordInputInvalid_: Boolean,

    /**
     * If either username or password entered incorrectly the save button will
     * be disabled.
     * @private
     * */
    isSaveButtonDisabled_: {
      type: Boolean,
      computed:
          'computeIsSaveButtonDisabled_(usernameInputInvalid_, passwordInputInvalid_)'
    }
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
    this.usernamesForSameOrigin =
        new Set(this.savedPasswords
                    .filter(
                        item => item.urls.shown === this.entry.urls.shown &&
                            (item.isPresentOnDevice() ===
                                 this.entry.isPresentOnDevice() ||
                             item.isPresentInAccount() ===
                                 this.entry.isPresentInAccount()))
                    .map(item => item.username));
  },

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  },

  /**
   * Helper function that checks entry isn't federation credential.
   * @return {boolean}
   * @private
   */
  computeIsEditDialog_() {
    return !this.entry.federationText;
  },

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   * @private
   */
  onCancel_() {
    this.close();
  },

  /**
   * Gets the password input's type. Should be 'text' when password is visible
   * or when there's federated text otherwise 'password'.
   * @return {string}
   * @private
   */
  getPasswordInputType_() {
    if (this.isEditDialog_) {
      return this.isPasswordVisible_ || this.entry.federationText ? 'text' :
                                                                    'password';
    } else {
      return this.getPasswordInputType();
    }
  },

  /**
   * Gets the title text for the show/hide icon.
   * @param {string} password
   * @param {boolean} isPasswordVisible
   * @param {string} hide The i18n text to use for 'Hide'
   * @param {string} show The i18n text to use for 'Show'
   * @private
   */
  showPasswordTitle_(password, isPasswordVisible, hide, show) {
    if (this.isEditDialog_) {
      return isPasswordVisible ? hide : show;
    } else {
      return this.showPasswordTitle(password, hide, show);
    }
  },

  /**
   * Get the right icon to display when hiding/showing a password.
   * @return {string}
   * @private
   */
  getIconClass_() {
    if (this.isEditDialog_) {
      return this.isPasswordVisible_ ? 'icon-visibility-off' :
                                       'icon-visibility';
    } else {
      return this.getIconClass();
    }
  },

  /**
   * Gets the text of the password. Will use the value of |entry.password|
   * unless it cannot be shown, in which case it will be a fixed number of
   * spaces. It can also be the federated text.
   * @return {string}
   * @private
   */
  getPassword_() {
    if (this.isEditDialog_) {
      return this.entry.password;
    } else {
      return this.getPassword();
    }
  },

  /**
   * Handler for tapping the show/hide button.
   * @private
   */
  onShowPasswordButtonTap_() {
    if (this.isEditDialog_) {
      this.isPasswordVisible_ = !this.isPasswordVisible_;
    } else {
      this.onShowPasswordButtonTap();
    }
  },

  /**
   * Handler for tapping the 'done' or 'save' button depending on isEditDialog_.
   * For 'save' button it should save new password. After pressing action button
   * the edit dialog should be closed.
   * @private
   */
  onActionButtonTap_() {
    if (this.isEditDialog_) {
      const idsToChange = [];
      const accountId = this.entry.accountId;
      const deviceId = this.entry.deviceId;
      if (accountId !== null) {
        idsToChange.push(accountId);
      }
      if (deviceId !== null) {
        idsToChange.push(deviceId);
      }

      PasswordManagerImpl.getInstance()
          .changeSavedPassword(
              idsToChange, this.$.usernameInput.value,
              this.$.passwordInput.value)
          .finally(() => {
            this.close();
          });
    } else {
      this.close();
    }
  },

  /**
   * @return {string}
   * @private
   */
  getActionButtonName_() {
    return this.isEditDialog_ ? this.i18n('save') : this.i18n('done');
  },

  /**
   * Manually de-select texts for readonly inputs.
   * @private
   */
  onInputBlur_() {
    this.shadowRoot.getSelection().removeAllRanges();
  },

  /**
   * Gets the HTML-formatted message to indicate in which locations the password
   * is stored.
   * @private
   */
  getStorageDetailsMessage_() {
    if (this.entry.isPresentInAccount() && this.entry.isPresentOnDevice()) {
      return this.i18n('passwordStoredInAccountAndOnDevice');
    }
    return this.entry.isPresentInAccount() ?
        this.i18n('passwordStoredInAccount') :
        this.i18n('passwordStoredOnDevice');
  },

  /**
   * @return {string}
   * @private
   */
  getTitle_() {
    return this.isEditDialog_ ? this.i18n('editPasswordTitle') :
                                this.i18n('passwordDetailsTitle');
  },

  /**
   * @return {string} The text to be displayed as the dialog's footnote.
   * @private
   */
  getFootnote_() {
    return this.i18n('editPasswordFootnote', this.entry.urls.shown);
  },

  /**
   * Helper function that checks if save button should be disabled.
   * @return {boolean}
   * @private
   */
  computeIsSaveButtonDisabled_() {
    return this.usernameInputInvalid_ || this.passwordInputInvalid_;
  },

  /**
   * Helper function that checks whether edited username is not used for the
   * same website.
   * @private
   */
  validateUsername_() {
    if (this.entry.username !== this.$.usernameInput.value) {
      this.usernameInputInvalid_ =
          this.usernamesForSameOrigin.has(this.$.usernameInput.value);
    } else {
      this.usernameInputInvalid_ = false;
    }
  },

});
