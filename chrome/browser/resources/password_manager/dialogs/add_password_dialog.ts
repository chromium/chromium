// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrTextareaElement} from 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {Page, Router} from '../router.js';
import {ShowPasswordMixinLit} from '../show_password_mixin_lit.js';
import {UserUtilMixinLit} from '../user_utils_mixin_lit.js';

import {getCss} from './add_password_dialog.css.js';
import {getHtml} from './add_password_dialog.html.js';

/**
 * Represents different user interactions related to adding credential from the
 * settings. Should be kept in sync with
 * |metrics_util::AddCredentialFromSettingsUserInteractions|. These values are
 * persisted to logs. Entries should not be renumbered and numeric values should
 * never be reused.
 */
export enum AddCredentialFromSettingsUserInteractions {
  // Used when the add credential dialog is opened from the settings.
  ADD_DIALOG_OPENED = 0,
  // Used when the add credential dialog is closed from the settings.
  ADD_DIALOG_CLOSED = 1,
  // Used when a new credential is added from the settings .
  CREDENTIAL_ADDED = 2,
  // Used when a new credential is being added from the add credential dialog in
  // settings and another credential exists with the same username/website
  // combination.
  DUPLICATED_CREDENTIAL_ENTERED = 3,
  // Used when an existing credential is viewed while adding a new credential
  // from the settings.
  DUPLICATE_CREDENTIAL_VIEWED = 4,
  // Must be last.
  COUNT = 5,
}

function recordAddCredentialInteraction(
    interaction: AddCredentialFromSettingsUserInteractions) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.AddCredentialFromSettings.UserAction2', interaction,
      AddCredentialFromSettingsUserInteractions.COUNT);
}

/**
 * Should be kept in sync with
 * |password_manager::metrics_util::PasswordNoteAction|.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum PasswordNoteAction {
  NOTE_ADDED_IN_ADD_DIALOG = 0,
  NOTE_ADDED_IN_EDIT_DIALOG = 1,
  NOTE_EDITED_IN_EDIT_DIALOG = 2,
  NOTE_REMOVED_IN_EDIT_DIALOG = 3,
  NOTE_NOT_CHANGED = 4,
  // Must be last.
  COUNT = 5,
}

export function recordPasswordNoteAction(action: PasswordNoteAction) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.PasswordNoteActionInSettings2', action,
      PasswordNoteAction.COUNT);
}

export interface AddPasswordDialogElement {
  $: {
    addButton: CrButtonElement,
    dialog: CrDialogElement,
    noteInput: CrTextareaElement,
    passwordInput: CrInputElement,
    showPasswordButton: CrIconButtonElement,
    storePicker: HTMLSelectElement,
    usernameInput: CrInputElement,
    viewExistingPasswordLink: HTMLAnchorElement,
    websiteInput: CrInputElement,
  };
}

/**
 * When user enters more than or equal to 900 characters in the note field, a
 * footer will be displayed below the note to warn the user.
 */
export const PASSWORD_NOTE_WARNING_CHARACTER_COUNT = 900;

/**
 * When user enters more than 1000 characters, the note will become invalid and
 * save button will be disabled.
 */
export const PASSWORD_NOTE_MAX_CHARACTER_COUNT = 1000;

const AddPasswordDialogElementBase =
    UserUtilMixinLit(ShowPasswordMixinLit(I18nMixinLit(CrLitElement)));

function getUsernamesByOrigin(
    passwords: chrome.passwordsPrivate.PasswordUiEntry[]):
    Map<string, Set<string>> {
  // Group existing usernames by signonRealm.
  return passwords.reduce(function(usernamesByOrigin, entry) {
    assert(entry.affiliatedDomains);
    for (const domain of entry.affiliatedDomains) {
      if (!usernamesByOrigin.has(domain.signonRealm)) {
        usernamesByOrigin.set(domain.signonRealm, new Set());
      }
      usernamesByOrigin.get(domain.signonRealm).add(entry.username);
    }
    return usernamesByOrigin;
  }, new Map());
}

export class AddPasswordDialogElement extends AddPasswordDialogElementBase {
  static get is() {
    return 'add-password-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      website_: {type: String},
      username_: {type: String},
      password_: {type: String},
      note_: {type: String},
      urlCollection_: {type: Object},
      usernamesBySignonRealm_: {type: Object},

      /**
       * Error message if the website input is invalid.
       */
      websiteErrorMessage_: {type: String},

      usernameErrorMessage_: {type: String},
      isPasswordInvalid_: {type: Boolean},
      canAddPassword_: {type: Boolean},
    };
  }

  protected accessor website_: string = '';
  protected accessor username_: string = '';
  protected accessor password_: string = '';
  protected accessor note_: string = '';
  private accessor usernamesBySignonRealm_: Map<string, Set<string>> =
      new Map();
  protected accessor websiteErrorMessage_: string|null = null;
  protected accessor usernameErrorMessage_: string|null = null;
  protected accessor canAddPassword_: boolean = false;
  protected accessor isPasswordInvalid_: boolean = false;
  private accessor urlCollection_: chrome.passwordsPrivate.UrlCollection|null =
      null;
  protected readonly storeOptionAccountValue_: string =
      chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
  protected readonly storeOptionDeviceValue_: string =
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE;

  private setSavedPasswordsListener_: (
      (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void)|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.setSavedPasswordsListener_ = passwordList => {
      this.usernamesBySignonRealm_ = getUsernamesByOrigin(passwordList);
    };

    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    recordAddCredentialInteraction(
        AddCredentialFromSettingsUserInteractions.ADD_DIALOG_OPENED);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    this.setSavedPasswordsListener_ = null;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('urlCollection_') ||
        changedPrivateProperties.has('username_') ||
        changedPrivateProperties.has('usernamesBySignonRealm_')) {
      this.usernameErrorMessage_ = this.computeUsernameErrorMessage_();
    }

    if (changedPrivateProperties.has('websiteErrorMessage_') ||
        changedPrivateProperties.has('website_') ||
        changedPrivateProperties.has('urlCollection_') ||
        changedPrivateProperties.has('username_') ||
        changedPrivateProperties.has('usernamesBySignonRealm_') ||
        changedPrivateProperties.has('password_') ||
        changedPrivateProperties.has('note_')) {
      this.canAddPassword_ = this.computeCanAddPassword_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('isAccountStoreUser')) {
      this.updateDefaultStore_();
    }
  }

  private updateDefaultStore_() {
    if (this.isAccountStoreUser) {
      this.$.storePicker.value = this.storeOptionAccountValue_;
    }
  }

  protected onWebsiteValueChanged_(e: CustomEvent<{value: string}>) {
    this.website_ = e.detail.value;
  }

  protected onUsernameValueChanged_(e: CustomEvent<{value: string}>) {
    this.username_ = e.detail.value;
  }

  protected onPasswordValueChanged_(e: CustomEvent<{value: string}>) {
    this.password_ = e.detail.value;
  }

  protected onNoteValueChanged_(e: CustomEvent<{value: string}>) {
    this.note_ = e.detail.value;
  }

  protected onCancelClick_() {
    this.closeDialog_();
  }

  private closeDialog_() {
    recordAddCredentialInteraction(
        AddCredentialFromSettingsUserInteractions.ADD_DIALOG_CLOSED);
    this.$.dialog.close();
  }

  /**
   * Helper function that checks whether the entered url is valid.
   */
  protected onWebsiteInput_() {
    if (this.website_.length === 0) {
      this.websiteErrorMessage_ = null;
      return;
    }
    PasswordManagerImpl.getInstance()
        .getUrlCollection(this.website_)
        .then(urlCollection => {
          this.urlCollection_ = urlCollection;
          this.websiteErrorMessage_ =
              !urlCollection ? this.i18n('notValidWebsite') : null;
        })
        .catch(() => this.websiteErrorMessage_ = this.i18n('notValidWebsite'));
  }

  protected onWebsiteInputBlur_() {
    if (this.website_.length === 0) {
      this.websiteErrorMessage_ = '';
    } else if (!this.websiteErrorMessage_ && !this.website_.includes('.')) {
      this.websiteErrorMessage_ =
          this.i18n('missingTLD', `${this.website_}.com`);
    }
  }

  protected isWebsiteInputInvalid_(): boolean {
    return this.websiteErrorMessage_ !== null;
  }

  protected showWebsiteError_(): boolean {
    return !!this.websiteErrorMessage_ && this.websiteErrorMessage_.length > 0;
  }

  private computeUsernameErrorMessage_(): string|null {
    const signonRealm = this.urlCollection_?.signonRealm;
    if (!signonRealm) {
      return null;
    }
    if (this.usernamesBySignonRealm_.has(signonRealm) &&
        this.usernamesBySignonRealm_.get(signonRealm)!.has(this.username_)) {
      recordAddCredentialInteraction(AddCredentialFromSettingsUserInteractions
                                         .DUPLICATED_CREDENTIAL_ENTERED);
      return this.i18n('usernameAlreadyUsed', this.website_);
    }
    return null;
  }

  protected doesUsernameExistAlready_(): boolean {
    return !!this.usernameErrorMessage_;
  }

  protected onPasswordInput_() {
    this.isPasswordInvalid_ = this.password_.length === 0;
  }

  protected onPasswordBlur_() {
    this.onPasswordInput_();
  }

  protected isNoteInputInvalid_(): boolean {
    return this.note_.length > PASSWORD_NOTE_MAX_CHARACTER_COUNT;
  }

  protected getFirstNoteFooter_(): string {
    return this.note_.length < PASSWORD_NOTE_WARNING_CHARACTER_COUNT ?
        '' :
        this.i18n(
            'passwordNoteCharacterCountWarning',
            PASSWORD_NOTE_MAX_CHARACTER_COUNT);
  }

  protected getSecondNoteFooter_(): string {
    return this.note_.length < PASSWORD_NOTE_WARNING_CHARACTER_COUNT ?
        '' :
        this.i18n(
            'passwordNoteCharacterCount', this.note_.length,
            PASSWORD_NOTE_MAX_CHARACTER_COUNT);
  }

  private computeCanAddPassword_(): boolean {
    if (this.isWebsiteInputInvalid_() || this.website_.length === 0) {
      return false;
    }
    if (this.doesUsernameExistAlready_()) {
      return false;
    }
    if (this.password_.length === 0) {
      return false;
    }
    if (this.isNoteInputInvalid_()) {
      return false;
    }
    return true;
  }

  protected onAddClick_() {
    assert(this.computeCanAddPassword_());
    assert(this.urlCollection_);
    recordAddCredentialInteraction(
        AddCredentialFromSettingsUserInteractions.CREDENTIAL_ADDED);
    const useAccountStore = this.isAccountStoreUser &&
        (this.$.storePicker.value === this.storeOptionAccountValue_);
    if (this.note_.trim()) {
      recordPasswordNoteAction(PasswordNoteAction.NOTE_ADDED_IN_ADD_DIALOG);
    }

    PasswordManagerImpl.getInstance()
        .addPassword({
          url: this.urlCollection_.signonRealm,
          username: this.username_,
          password: this.password_,
          note: this.note_,
          useAccountStore: useAccountStore,
        })
        .then(() => {
          this.closeDialog_();
        })
        .catch(() => {});
  }

  protected getViewExistingPasswordAriaDescription_(): string {
    return this.urlCollection_ ?
        this.i18n(
            'viewExistingPasswordAriaDescription', this.username_,
            this.urlCollection_.shown) :
        '';
  }

  protected onViewExistingPasswordClick_(e: Event) {
    recordAddCredentialInteraction(
        AddCredentialFromSettingsUserInteractions.DUPLICATE_CREDENTIAL_VIEWED);
    e.preventDefault();
    Router.getInstance().navigateTo(
        Page.PASSWORD_DETAILS, this.urlCollection_?.shown);
    this.closeDialog_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'add-password-dialog': AddPasswordDialogElement;
  }
}

customElements.define(AddPasswordDialogElement.is, AddPasswordDialogElement);
