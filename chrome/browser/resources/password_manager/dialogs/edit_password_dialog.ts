// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import './add_password_dialog.js';

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

import {PASSWORD_NOTE_MAX_CHARACTER_COUNT, PASSWORD_NOTE_WARNING_CHARACTER_COUNT, PasswordNoteAction, recordPasswordNoteAction} from './add_password_dialog.js';
import {getCss} from './edit_password_dialog.css.js';
import {getHtml} from './edit_password_dialog.html.js';

export interface EditPasswordDialogElement {
  $: {
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    passwordInput: CrInputElement,
    passwordNote: CrTextareaElement,
    saveButton: CrButtonElement,
    showPasswordButton: CrIconButtonElement,
    usernameInput: CrInputElement,
    viewExistingPasswordLink: HTMLAnchorElement,
  };
}

/**
 * Computes possible conflicting username by finding all passwords with
 * matching signonRealms. Returns map where key is the username and value is
 * human readable representation of signonRealm. Username is considered
 * conflicting if shares any domain with |currentPassword|.
 */
function getConflictingUsernames(
    currentPassword: chrome.passwordsPrivate.PasswordUiEntry,
    passwords: chrome.passwordsPrivate.PasswordUiEntry[]): Map<string, string> {
  assert(currentPassword.affiliatedDomains);
  const currentSignonRealms =
      currentPassword.affiliatedDomains.map(domain => domain.signonRealm);
  return passwords.reduce(function(conflictingUsername, entry) {
    assert(entry.affiliatedDomains);
    const signonRealms =
        entry.affiliatedDomains.map(domain => domain.signonRealm);

    const signonRealm = signonRealms.filter(
        signonRealm => currentSignonRealms.includes(signonRealm))[0];
    if (signonRealm) {
      conflictingUsername.set(
          entry.username,
          entry.affiliatedDomains
              .find(domain => domain.signonRealm === signonRealm)!.name);
    }
    return conflictingUsername;
  }, new Map());
}

const EditPasswordDialogElementBase =
    ShowPasswordMixinLit(I18nMixinLit(CrLitElement));

export class EditPasswordDialogElement extends EditPasswordDialogElementBase {
  static get is() {
    return 'edit-password-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      credential: {type: Object},
      showRedirect: {type: Boolean},
      username_: {type: String},
      password_: {type: String},
      note_: {type: String},
      conflictingUsernames_: {type: Object},
      usernameErrorMessage_: {type: String},
      canEditPassword_: {type: Boolean},
    };
  }

  accessor credential: chrome.passwordsPrivate.PasswordUiEntry;
  accessor showRedirect: boolean = false;
  protected accessor username_: string = '';
  protected accessor password_: string = '';
  protected accessor note_: string = '';
  private accessor conflictingUsernames_: Map<string, string> = new Map();
  protected accessor usernameErrorMessage_: string|null = null;
  protected accessor canEditPassword_: boolean = false;
  private setSavedPasswordsListener_: (
      (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void)|null = null;

  override connectedCallback() {
    super.connectedCallback();

    assert(this.credential.password);
    this.username_ = this.credential.username;
    this.password_ = this.credential.password;
    this.note_ = this.credential.note ?? '';

    this.setSavedPasswordsListener_ = credentialList => {
      // Passkeys and federated credential may have the same username as a
      // password, since they can be different ways to authenticate the same
      // user. Thus, ignore these when finding conflicting usernames.
      const passwordList = credentialList.filter(
          credential => !credential.isPasskey && !credential.federationText);
      this.conflictingUsernames_ =
          getConflictingUsernames(this.credential, passwordList);
    };

    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
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
    if (changedProperties.has('credential') ||
        changedPrivateProperties.has('username_') ||
        changedPrivateProperties.has('conflictingUsernames_')) {
      this.usernameErrorMessage_ = this.computeUsernameErrorMessage_();
    }

    if (changedProperties.has('credential') ||
        changedPrivateProperties.has('username_') ||
        changedPrivateProperties.has('password_') ||
        changedPrivateProperties.has('note_') ||
        changedPrivateProperties.has('conflictingUsernames_')) {
      this.canEditPassword_ = this.computeCanEditPassword_();
    }
  }

  private computeUsernameErrorMessage_(): string|null {
    if (!this.conflictingUsernames_) {
      return null;
    }
    if (this.conflictingUsernames_.has(this.username_) &&
        this.username_ !== this.credential.username) {
      return this.i18n(
          'usernameAlreadyUsed',
          this.conflictingUsernames_.get(this.username_)!,
      );
    }
    return null;
  }

  protected doesUsernameExistAlready_(): boolean {
    return !!this.usernameErrorMessage_;
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
    this.$.dialog.close();
  }

  protected getFootnote_(): string {
    assert(this.credential.affiliatedDomains);
    return this.i18n(
        'editPasswordFootnote',
        this.credential.affiliatedDomains[0]?.name ?? '');
  }

  protected showRedirect_(): boolean {
    return this.showRedirect && this.doesUsernameExistAlready_();
  }

  protected getViewExistingPasswordAriaDescription_(): string {
    if (!this.conflictingUsernames_) {
      return '';
    }
    return this.conflictingUsernames_.has(this.username_) ?
        this.i18n(
            'viewExistingPasswordAriaDescription', this.username_,
            this.conflictingUsernames_.get(this.username_)!) :
        '';
  }

  protected onViewExistingPasswordClick_(e: Event) {
    e.preventDefault();
    assert(this.conflictingUsernames_.has(this.username_));
    Router.getInstance().navigateTo(
        Page.PASSWORD_DETAILS, this.conflictingUsernames_.get(this.username_));
    this.$.dialog.close();
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

  private computeCanEditPassword_(): boolean {
    return !this.doesUsernameExistAlready_() && !!this.password_ &&
        this.password_.length > 0 && !this.isNoteInputInvalid_();
  }

  protected onEditClick_() {
    assert(this.computeCanEditPassword_());
    this.recordPasswordNoteMetrics();
    this.credential.password = this.password_;
    this.credential.username = this.username_;
    this.credential.note = this.note_;
    PasswordManagerImpl.getInstance()
        .changeCredential(this.credential)
        .finally(() => {
          this.$.dialog.close();
        });
  }

  private recordPasswordNoteMetrics() {
    const newNote = this.note_.trim();
    const oldNote = this.credential?.note || '';
    if (oldNote === newNote) {
      recordPasswordNoteAction(PasswordNoteAction.NOTE_NOT_CHANGED);
    } else if (oldNote !== '' && newNote !== '') {
      recordPasswordNoteAction(PasswordNoteAction.NOTE_EDITED_IN_EDIT_DIALOG);
    } else if (oldNote !== '') {
      recordPasswordNoteAction(PasswordNoteAction.NOTE_REMOVED_IN_EDIT_DIALOG);
    } else {
      recordPasswordNoteAction(PasswordNoteAction.NOTE_ADDED_IN_EDIT_DIALOG);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'edit-password-dialog': EditPasswordDialogElement;
  }
}

customElements.define(EditPasswordDialogElement.is, EditPasswordDialogElement);
