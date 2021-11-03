// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-edit-dialog' is the dialog that allows showing,
 * editing or adding a password.
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
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

interface PasswordEditDialogElement {
  $: {
    dialog: CrDialogElement,
    passwordInput: CrInputElement,
    storePicker: HTMLSelectElement,
    usernameInput: CrInputElement,
    websiteInput: CrInputElement,
  };
}

const PasswordEditDialogElementBase = I18nMixin(PolymerElement);

/**
 * Contains the possible modes for 'password-edit-dialog'.
 * VIEW: entry is an existing federation credential
 * EDIT: entry is an existing password
 * ADD: no existing entry
 */
enum PasswordDialogMode {
  VIEW = 'view',
  EDIT = 'edit',
  ADD = 'add',
}

/* TODO(crbug.com/1255127): Revisit usage for 3 different modes. */
class PasswordEditDialogElement extends PasswordEditDialogElementBase {
  static get is() {
    return 'password-edit-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Has value for dialog in VIEW and EDIT modes.
       */
      existingEntry: {type: Object, value: null},

      isAccountStoreUser: {type: Boolean, value: false},

      accountEmail: {type: String, value: null},

      storeOptionAccountValue: {type: String, value: 'account', readonly: true},

      storeOptionDeviceValue: {type: String, value: 'device', readonly: true},

      /**
       * Saved passwords after deduplicating versions that are repeated in the
       * account and on the device.
       */
      savedPasswords: {
        type: Array,
        value: () => [],
      },

      dialogMode_: {
        type: String,
        computed: 'computeDialogMode_(existingEntry)',
      },

      /**
       * True if existing entry is a federated credential.
       */
      isInViewMode_: {
        type: Boolean,
        computed: 'computeIsInViewMode_(dialogMode_)',
      },

      /**
       * Whether the password is visible or obfuscated.
       */
      isPasswordVisible_: {
        type: Boolean,
        value: false,
      },

      /**
       * Has value if entered website is a valid url.
       */
      websiteUrls_: {type: Object, value: null},

      /**
       * Whether the website input is invalid.
       */
      websiteInputInvalid_: {type: Boolean, value: false},

      /**
       * Error message if the website input is invalid.
       */
      websiteInputErrorMessage_: {type: String, value: null},

      /**
       * Current value in username input.
       */
      username_: {type: String, value: ''},

      /**
       * Whether the username input is invalid.
       */
      usernameInputInvalid_: {
        type: Boolean,
        computed:
            'computeUsernameInputInvalid_(username_, websiteUrls_.origin)',
      },

      /**
       * Current value in password input.
       */
      password_: {type: String, value: ''},

      /**
       * If either username or password entered incorrectly the save button will
       * be disabled.
       * */
      isSaveButtonDisabled_: {
        type: Boolean,
        computed:
            'computeIsSaveButtonDisabled_(websiteUrls_, usernameInputInvalid_, password_)'
      }
    };
  }

  existingEntry: MultiStorePasswordUiEntry|null;
  isAccountStoreUser: boolean;
  accountEmail: string|null;
  readonly storeOptionAccountValue: string;
  readonly storeOptionDeviceValue: string;
  savedPasswords: Array<MultiStorePasswordUiEntry>;
  private usernamesByOrigin_: Map<string, Set<string>>|null = null;
  private dialogMode_: PasswordDialogMode;
  private isInViewMode_: boolean;
  private isPasswordVisible_: boolean;
  private websiteUrls_: chrome.passwordsPrivate.UrlCollection|null;
  private websiteInputInvalid_: boolean;
  private websiteInputErrorMessage_: string|null;
  private username_: string;
  private usernameInputInvalid_: boolean;
  private password_: string;
  private isSaveButtonDisabled_: boolean;

  connectedCallback() {
    super.connectedCallback();
    if (this.existingEntry) {
      this.websiteUrls_ = this.existingEntry.urls;
      this.username_ = this.existingEntry.username;
    }
    this.password_ = this.getPassword_();
    if (!this.isInViewMode_) {
      this.usernamesByOrigin_ = this.getUsernamesByOrigin_();
    }
    if (this.shouldShowStorePicker_()) {
      PasswordManagerImpl.getInstance().isAccountStoreDefault().then(
          isAccountStoreDefault => {
            this.$.storePicker.value = isAccountStoreDefault ?
                this.storeOptionAccountValue :
                this.storeOptionDeviceValue;
          });
    }
  }

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  }

  private computeDialogMode_(): PasswordDialogMode {
    if (this.existingEntry) {
      return this.existingEntry.federationText ? PasswordDialogMode.VIEW :
                                                 PasswordDialogMode.EDIT;
    }

    return PasswordDialogMode.ADD;
  }

  private computeIsInViewMode_(): boolean {
    return this.dialogMode_ === PasswordDialogMode.VIEW;
  }

  private computeIsSaveButtonDisabled_(): boolean {
    return !this.websiteUrls_ || this.usernameInputInvalid_ ||
        !this.password_.length;
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
    // VIEW mode implies |existingEntry| is a federated credential.
    if (this.isInViewMode_) {
      return 'text';
    }

    return this.isPasswordVisible_ ? 'text' : 'password';
  }

  /**
   * Gets the title text for the show/hide icon.
   */
  private showPasswordTitle_(): string {
    assert(!this.isInViewMode_);
    return this.isPasswordVisible_ ? this.i18n('hidePassword') :
                                     this.i18n('showPassword');
  }

  /**
   * Get the right icon to display when hiding/showing a password.
   */
  private getIconClass_(): string {
    assert(!this.isInViewMode_);
    return this.isPasswordVisible_ ? 'icon-visibility-off' : 'icon-visibility';
  }

  /**
   * Gets the initial text to show in the website input.
   */
  private getWebsite_(): string {
    return this.dialogMode_ === PasswordDialogMode.ADD ?
        '' :
        this.existingEntry!.urls.link;
  }

  /**
   * Gets the initial text to show in the password input: the password for a
   * regular credential, the federation text for a federated credential or empty
   * string in the ADD mode.
   */
  private getPassword_(): string {
    switch (this.dialogMode_) {
      case PasswordDialogMode.VIEW:
        // VIEW mode implies |existingEntry| is a federated credential.
        return this.existingEntry!.federationText!;
      case PasswordDialogMode.EDIT:
        return this.existingEntry!.password;
      case PasswordDialogMode.ADD:
        return '';
      default:
        assertNotReached();
        return '';
    }
  }

  /**
   * Handler for tapping the show/hide button.
   */
  private onShowPasswordButtonTap_() {
    assert(!this.isInViewMode_);
    this.isPasswordVisible_ = !this.isPasswordVisible_;
  }

  /**
   * Handler for tapping the 'done' or 'save' button depending on |dialogMode_|.
   * For 'save' button it should save new password. After pressing action button
   * the edit dialog should be closed.
   */
  private onActionButtonTap_() {
    switch (this.dialogMode_) {
      case PasswordDialogMode.VIEW:
        this.close();
        return;
      case PasswordDialogMode.EDIT:
        this.changePassword_();
        return;
      case PasswordDialogMode.ADD:
        this.addPassword_();
        return;
      default:
        assertNotReached();
    }
  }

  private addPassword_() {
    const useAccountStore = !this.$.storePicker.hidden ?
        (this.$.storePicker.value === this.storeOptionAccountValue) :
        false;
    PasswordManagerImpl.getInstance()
        .addPassword({
          url: this.$.websiteInput.value,
          username: this.username_,
          password: this.password_,
          useAccountStore: useAccountStore
        })
        .finally(() => {
          this.close();
        });
  }

  private changePassword_() {
    const idsToChange = [];
    const accountId = this.existingEntry!.accountId;
    const deviceId = this.existingEntry!.deviceId;
    if (accountId !== null) {
      idsToChange.push(accountId);
    }
    if (deviceId !== null) {
      idsToChange.push(deviceId);
    }

    PasswordManagerImpl.getInstance()
        .changeSavedPassword(idsToChange, this.username_, this.password_)
        .finally(() => {
          this.close();
        });
  }

  private getActionButtonName_(): string {
    return this.isInViewMode_ ? this.i18n('done') : this.i18n('save');
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
  private getStorageDetailsMessage_(): string {
    if (this.dialogMode_ === PasswordDialogMode.ADD) {
      // Storage message is not shown in the ADD mode.
      return '';
    }
    if (this.existingEntry!.isPresentInAccount() &&
        this.existingEntry!.isPresentOnDevice()) {
      return this.i18n('passwordStoredInAccountAndOnDevice');
    }
    return this.existingEntry!.isPresentInAccount() ?
        this.i18n('passwordStoredInAccount') :
        this.i18n('passwordStoredOnDevice');
  }

  private getStoreOptionAccountText_(): string {
    if (this.dialogMode_ !== PasswordDialogMode.ADD) {
      // Store picker is only shown in the ADD mode.
      return '';
    }

    return this.i18n('addPasswordStoreOptionAccount', this.accountEmail!);
  }

  private getTitle_(): string {
    switch (this.dialogMode_) {
      case PasswordDialogMode.ADD:
        return this.i18n('addPasswordTitle');
      case PasswordDialogMode.EDIT:
        return this.i18n('editPasswordTitle');
      case PasswordDialogMode.VIEW:
        return this.i18n('passwordDetailsTitle');
      default:
        assertNotReached();
        return '';
    }
  }

  private shouldShowStorageDetails_(): boolean {
    return this.dialogMode_ !== PasswordDialogMode.ADD &&
        this.isAccountStoreUser;
  }

  private shouldShowStorePicker_(): boolean {
    return this.dialogMode_ === PasswordDialogMode.ADD &&
        this.isAccountStoreUser;
  }

  private isWebsiteEditable_(): boolean {
    return this.dialogMode_ === PasswordDialogMode.ADD;
  }

  /**
   * @return The text to be displayed as the dialog's footnote.
   */
  private getFootnote_(): string {
    return this.dialogMode_ === PasswordDialogMode.ADD ?
        this.i18n('addPasswordFootnote') :
        this.i18n('editPasswordFootnote', this.existingEntry!.urls.shown);
  }

  private getClassForWebsiteInput_(): string {
    // If website input is empty, it is invalid, but has no error message.
    // To handle this, the error is shown only if 'has-error-message' is set.
    return this.websiteInputErrorMessage_ ? 'has-error-message' : '';
  }

  /**
   * Helper function that checks whether the entered url is valid.
   */
  private validateWebsite_() {
    assert(this.dialogMode_ === PasswordDialogMode.ADD);
    if (!this.$.websiteInput.value.length) {
      this.websiteUrls_ = null;
      this.websiteInputErrorMessage_ = null;
      this.websiteInputInvalid_ = true;
      return;
    }
    PasswordManagerImpl.getInstance()
        .getUrlCollection(this.$.websiteInput.value)
        .then(urlCollection => {
          if (urlCollection) {
            this.websiteUrls_ = urlCollection;
            this.websiteInputErrorMessage_ = null;
            this.websiteInputInvalid_ = false;
          } else {
            this.websiteUrls_ = null;
            this.websiteInputErrorMessage_ = this.i18n('notValidWebAddress');
            this.websiteInputInvalid_ = true;
          }
        });
  }

  /**
   * Checks whether edited username is not used for the same website.
   */
  private computeUsernameInputInvalid_(): boolean {
    if (this.isInViewMode_ || !this.websiteUrls_ || !this.usernamesByOrigin_) {
      return false;
    }
    if (this.dialogMode_ === PasswordDialogMode.EDIT &&
        this.username_ === this.existingEntry!.username) {
      // The value hasn't changed.
      return false;
    }
    // TODO(crbug.com/1264468): Consider moving duplication check to backend.
    return this.usernamesByOrigin_.has(this.websiteUrls_.origin) &&
        this.usernamesByOrigin_.get(this.websiteUrls_.origin)!.has(
            this.username_);
  }

  /**
   * Used for the fast check whether edited username is already used.
   */
  private getUsernamesByOrigin_(): Map<string, Set<string>> {
    assert(!this.isInViewMode_);
    const relevantPasswords = this.dialogMode_ === PasswordDialogMode.EDIT ?
        // In EDIT mode entries considered duplicates only if in the same store.
        this.savedPasswords.filter(item => {
          return item.isPresentOnDevice() ===
              this.existingEntry!.isPresentOnDevice() ||
              item.isPresentInAccount() ===
              this.existingEntry!.isPresentInAccount();
        }) :
        // In ADD mode entries considered duplicates irrespective of the store.
        this.savedPasswords;

    // Group existing usernames by origin.
    return relevantPasswords.reduce(function(usernamesByOrigin, entry) {
      const origin = entry.urls.origin;
      if (!usernamesByOrigin.has(origin)) {
        usernamesByOrigin.set(origin, new Set());
      }
      usernamesByOrigin.get(origin).add(entry.username);
      return usernamesByOrigin;
    }, new Map());
  }
}

customElements.define(PasswordEditDialogElement.is, PasswordEditDialogElement);
