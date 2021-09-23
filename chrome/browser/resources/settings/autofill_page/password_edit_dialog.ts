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

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

interface PasswordEditDialogElement {
  $: {
    dialog: CrDialogElement,
    usernameInput: CrInputElement,
    passwordInput: CrInputElement,
  };
}

const PasswordEditDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior};

class PasswordEditDialogElement extends PasswordEditDialogElementBase {
  static get is() {
    return 'password-edit-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      entry: Object,

      shouldShowStorageDetails: {type: Boolean, value: false},

      /**
       * Saved passwords after deduplicating versions that are repeated in the
       * account and on the device.
       */
      savedPasswords: {
        type: Array,
        value: () => [],
      },

      /**
       * Usernames for the same website. Used for the fast check whether edited
       * username is already used.
       */
      usernamesForSameOrigin_: {
        type: Object,
        value: null,
      },

      /**
       * Check if entry isn't federation credential.
       */
      isEditDialog_: {
        type: Boolean,
        computed: 'computeIsEditDialog_(entry)',
      },

      /**
       * Whether the password is visible or obfuscated.
       */
      isPasswordVisible_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the username input is invalid.
       */
      usernameInputInvalid_: Boolean,

      /**
       * Whether the password input is invalid.
       */
      passwordInputInvalid_: Boolean,

      /**
       * If either username or password entered incorrectly the save button will
       * be disabled.
       * */
      isSaveButtonDisabled_: {
        type: Boolean,
        computed:
            'computeIsSaveButtonDisabled_(usernameInputInvalid_, passwordInputInvalid_)'
      }

    };
  }

  shouldShowStorageDetails: boolean;
  entry: MultiStorePasswordUiEntry;
  savedPasswords: Array<MultiStorePasswordUiEntry>;
  private usernamesForSameOrigin_: Set<string>|null;
  private isEditDialog_: boolean;
  private isPasswordVisible_: boolean;
  private usernameInputInvalid_: boolean;
  private passwordInputInvalid_: boolean;
  private isSaveButtonDisabled_: boolean;

  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
    this.usernamesForSameOrigin_ =
        new Set(this.savedPasswords
                    .filter(
                        item => item.urls.shown === this.entry.urls.shown &&
                            (item.isPresentOnDevice() ===
                                 this.entry.isPresentOnDevice() ||
                             item.isPresentInAccount() ===
                                 this.entry.isPresentInAccount()))
                    .map(item => item.username));
  }

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  }

  /**
   * Helper function that checks entry isn't federation credential.
   */
  private computeIsEditDialog_(): boolean {
    return !this.entry.federationText;
  }

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   */
  private onCancel_() {
    this.close();
  }

  /**
   * Gets the password input's type. Should be 'text' when input content is
   * visible otherwise 'password'. If the entry is a federated credential,
   * the content (federation text) is always visible.
   */
  private getPasswordInputType_(): string {
    // Not edit dialog implies a view dialog for a federated credential.
    if (!this.isEditDialog_) {
      return 'text';
    }

    return this.isPasswordVisible_ ? 'text' : 'password';
  }

  /**
   * Gets the title text for the show/hide icon, visible only in edit dialog.
   */
  private showPasswordTitle_(): string {
    return this.isPasswordVisible_ ? this.i18n('hidePassword') :
                                     this.i18n('showPassword');
  }

  /**
   * Get the right icon to display when hiding/showing a password, visible
   * only in edit dialog.
   */
  private getIconClass_(): string {
    return this.isPasswordVisible_ ? 'icon-visibility-off' : 'icon-visibility';
  }

  /**
   * Gets the text to show in the password input: the password for a regular
   * credential or the federation text for a federated credential.
   */
  private getPassword_(): string {
    return this.isEditDialog_ ?
        this.entry.password :
        // Not edit dialog implies a view dialog for a federated credential.
        this.entry.federationText!;
  }

  /**
   * Handler for tapping the show/hide button, visible only in edit dialog.
   */
  private onShowPasswordButtonTap_() {
    this.isPasswordVisible_ = !this.isPasswordVisible_;
  }

  /**
   * Handler for tapping the 'done' or 'save' button depending on isEditDialog_.
   * For 'save' button it should save new password. After pressing action button
   * the edit dialog should be closed.
   */
  private onActionButtonTap_() {
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
  }

  private getActionButtonName_(): string {
    return this.isEditDialog_ ? this.i18n('save') : this.i18n('done');
  }

  /**
   * Manually de-select texts for readonly inputs.
   */
  private onInputBlur_() {
    this.shadowRoot!.getSelection()!.removeAllRanges();
  }

  /**
   * Gets the HTML-formatted message to indicate in which locations the password
   * is stored.
   */
  private getStorageDetailsMessage_() {
    if (this.entry.isPresentInAccount() && this.entry.isPresentOnDevice()) {
      return this.i18n('passwordStoredInAccountAndOnDevice');
    }
    return this.entry.isPresentInAccount() ?
        this.i18n('passwordStoredInAccount') :
        this.i18n('passwordStoredOnDevice');
  }

  private getTitle_(): string {
    return this.isEditDialog_ ? this.i18n('editPasswordTitle') :
                                this.i18n('passwordDetailsTitle');
  }

  /**
   * @return The text to be displayed as the dialog's footnote.
   */
  private getFootnote_(): string {
    return this.i18n('editPasswordFootnote', this.entry.urls.shown);
  }

  /**
   * Helper function that checks if save button should be disabled.
   */
  private computeIsSaveButtonDisabled_(): boolean {
    return this.usernameInputInvalid_ || this.passwordInputInvalid_;
  }

  /**
   * Helper function that checks whether edited username is not used for the
   * same website.
   */
  private validateUsername_() {
    if (this.entry.username !== this.$.usernameInput.value) {
      this.usernameInputInvalid_ =
          this.usernamesForSameOrigin_!.has(this.$.usernameInput.value);
    } else {
      this.usernameInputInvalid_ = false;
    }
  }
}

customElements.define(PasswordEditDialogElement.is, PasswordEditDialogElement);
