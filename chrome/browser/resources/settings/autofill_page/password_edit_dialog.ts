// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-edit-dialog' is the dialog that allows showing,
 * editing or adding a password.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import '../icons.html.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import './passwords_shared.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './password_edit_dialog.html.js';
import {PasswordListItemElement} from './password_list_item.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {PasswordRequestorMixin} from './password_requestor_mixin.js';
import {PASSWORD_VIEW_PAGE_REQUESTED_EVENT_NAME} from './password_view.js';

export type SavedPasswordEditedEvent =
    CustomEvent<chrome.passwordsPrivate.PasswordUiEntry>;

const SAVED_PASSWORD_EDITED_EVENT_NAME = 'saved-password-edited';

declare global {
  interface HTMLElementEventMap {
    [SAVED_PASSWORD_EDITED_EVENT_NAME]: SavedPasswordEditedEvent;
  }
}

export interface PasswordEditDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancel: CrButtonElement,
    dialog: CrDialogElement,
    footnote: HTMLElement,
    passwordInput: CrInputElement,
    storageDetails: HTMLElement,
    storePicker: HTMLSelectElement,
    title: HTMLElement,
    usernameInput: CrInputElement,
    viewExistingPasswordLink: HTMLElement,
    websiteInput: CrInputElement,
  };
}

/**
 * Should be kept in sync with
 * |password_manager::metrics_util::PasswordNoteAction|.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
enum PasswordNoteAction {
  NOTE_ADDED_IN_ADD_DIALOG = 0,
  NOTE_ADDED_IN_EDIT_DIALOG = 1,
  NOTE_EDITED_IN_EDIT_DIALOG = 2,
  NOTE_REMOVED_IN_EDIT_DIALOG = 3,
  NOTE_NOT_CHANGED = 4,
  // Must be last.
  COUNT = 5,
}

function recordPasswordNoteAction(action: PasswordNoteAction) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.PasswordNoteActionInSettings2', action,
      PasswordNoteAction.COUNT);
}

const PasswordEditDialogElementBase =
    PasswordRequestorMixin(I18nMixin(PolymerElement));

/**
 * When user enters more than or equal to 900 characters in the note field, a
 * footer will be displayed below the note to warn the user.
 */
const PASSWORD_NOTE_WARNING_CHARACTER_COUNT = 900;

/**
 * When user enters more than 1000 characters, the note will become invalid and
 * save button will be disabled.
 */
const PASSWORD_NOTE_MAX_CHARACTER_COUNT = 1000;

/**
 * Contains the possible modes for 'password-edit-dialog'.
 * FEDERATED_VIEW: entry is an existing federated credential
 * EDIT: entry is an existing password and in edit mode
 * ADD: no existing entry
 */
export enum PasswordDialogMode {
  FEDERATED_VIEW = 'federated_view',
  EDIT = 'edit',
  ADD = 'add',
}

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

/* TODO(crbug.com/1255127): Revisit usage for 3 different modes. */
export class PasswordEditDialogElement extends PasswordEditDialogElementBase {
  static get is() {
    return 'password-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Has value for dialog in FEDERATED_VIEW or EDIT modes.
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

      dialogMode: {
        type: String,
        computed: 'computeDialogMode_(existingEntry)',
        reflectToAttribute: true,
      },

      /**
       * True if existing entry is a federated credential.
       */
      isInFederatedViewMode_: {
        type: Boolean,
        computed: 'computeIsInFederatedViewMode_(dialogMode)',
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
       * Current value in note field.
       */
      note_: {type: String, value: ''},

      /**
       * Password note can be at most PASSWORD_NOTE_MAX_CHARACTER_COUNT
       * characters long. Warns users when they start having more than or equal
       * to PASSWORD_NOTE_WARNING_CHARACTER_COUNT characters.
       */
      noteFirstFooter_: {
        type: String,
        computed: 'computeNoteFirstFooter_(dialogMode, note_)',
      },

      /**
       * Informs the user about the note's character count and the character
       * limit.
       */
      noteSecondFooter_: {
        type: String,
        computed: 'computeNoteSecondFooter_(dialogMode, note_)',
      },

      /**
       * Whether the note is longer than PASSWORD_NOTE_MAX_CHARACTER_COUNT
       * characters.
       */
      noteInvalid_: {
        type: Boolean,
        computed: 'computeNoteInvalid_(dialogMode, note_)',
      },

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
       * If either website, username or password entered incorrectly the save
       * button will be disabled.
       * */
      isSaveButtonDisabled_: {
        type: Boolean,
        computed:
            'computeIsSaveButtonDisabled_(websiteUrls_, websiteInputInvalid_, ' +
            'usernameInputInvalid_, password_, noteInvalid_, ' +
            'isPasswordNotesEnabled_)',
      },

      /* If true, note field will be shown and used when saving the password. */
      isPasswordNotesEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePasswordNotes');
        },
      },

      /* Enables dispatching change events and extending auth. */
      isPasswordViewPageEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePasswordViewPage');
        },
      },
    };
  }

  static get observers() {
    return [
      'extendAuthValidityOnUserInteraction_(username_, password_, note_)',
    ];
  }

  existingEntry: chrome.passwordsPrivate.PasswordUiEntry|null;
  isAccountStoreUser: boolean;
  accountEmail: string|null;
  readonly storeOptionAccountValue: string;
  readonly storeOptionDeviceValue: string;
  savedPasswords: chrome.passwordsPrivate.PasswordUiEntry[];
  private usernamesByOrigin_: Map<string, Set<string>>|null = null;
  dialogMode: PasswordDialogMode;
  private isInFederatedViewMode_: boolean;
  private isPasswordVisible_: boolean;
  private websiteUrls_: chrome.passwordsPrivate.UrlCollection|null;
  private websiteInputInvalid_: boolean;
  private websiteInputErrorMessage_: string|null;
  private username_: string;
  private note_: string;
  private noteFirstFooter_: string;
  private noteSecondFooter_: string;
  private noteInvalid_: boolean;
  private usernameInputInvalid_: boolean;
  private password_: string;
  private isSaveButtonDisabled_: boolean;
  private isPasswordNotesEnabled_: boolean;
  private isPasswordViewPageEnabled_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.initDialog_();
  }

  // <if expr="is_chromeos">
  override onPasswordPromptClose(event: CloseEvent) {
    super.onPasswordPromptClose(event);
    // In chromeOS closing the password prompt dialog while the edit dialog is
    // open also closes the edit dialog. This happens when add dialog wants to
    // switch to edit dialog.
    event.stopPropagation();
  }
  // </if>

  private initDialog_() {
    if (this.existingEntry) {
      this.websiteUrls_ = this.existingEntry.urls;
      this.username_ = this.existingEntry.username;
      this.note_ = this.existingEntry.note || '';
    }
    this.password_ = this.getPassword_();
    if (!this.isInFederatedViewMode_) {
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
    this.isPasswordVisible_ = false;
  }

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  }

  private computeDialogMode_(): PasswordDialogMode {
    if (this.existingEntry) {
      return this.existingEntry.federationText ?
          PasswordDialogMode.FEDERATED_VIEW :
          PasswordDialogMode.EDIT;
    }

    return PasswordDialogMode.ADD;
  }

  private computeIsInFederatedViewMode_(): boolean {
    return this.dialogMode === PasswordDialogMode.FEDERATED_VIEW;
  }

  private computeIsSaveButtonDisabled_(): boolean {
    return !this.websiteUrls_ || this.websiteInputInvalid_ ||
        this.usernameInputInvalid_ || !this.password_.length ||
        (this.isPasswordNotesEnabled_ && this.noteInvalid_);
  }

  private shouldShowNote_(): boolean {
    return this.isPasswordNotesEnabled_ &&
        (this.dialogMode === PasswordDialogMode.EDIT ||
         this.dialogMode === PasswordDialogMode.ADD);
  }

  private isNoteLongerThanOrEqualTo_(characterCount: number): boolean {
    return (this.dialogMode === PasswordDialogMode.EDIT ||
            this.dialogMode === PasswordDialogMode.ADD) &&
        this.note_.length >= characterCount;
  }

  private computeNoteFirstFooter_(): string {
    return this.isNoteLongerThanOrEqualTo_(
               PASSWORD_NOTE_WARNING_CHARACTER_COUNT) ?
        this.i18n(
            'passwordNoteCharacterCountWarning',
            PASSWORD_NOTE_MAX_CHARACTER_COUNT) :
        '';
  }

  private computeNoteSecondFooter_(): string {
    return this.isNoteLongerThanOrEqualTo_(
               PASSWORD_NOTE_WARNING_CHARACTER_COUNT) ?
        this.i18n(
            'passwordNoteCharacterCount', this.note_.length,
            PASSWORD_NOTE_MAX_CHARACTER_COUNT) :
        '';
  }

  private computeNoteInvalid_(): boolean {
    return this.isNoteLongerThanOrEqualTo_(
        PASSWORD_NOTE_MAX_CHARACTER_COUNT + 1);
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
    // FEDERATED_VIEW mode implies |existingEntry| is a federated credential.
    if (this.isInFederatedViewMode_) {
      return 'text';
    }

    return this.isPasswordVisible_ ? 'text' : 'password';
  }

  /**
   * Gets the title text for the show/hide icon.
   */
  private showPasswordTitle_(): string {
    assert(!this.isInFederatedViewMode_);
    return this.isPasswordVisible_ ? this.i18n('hidePassword') :
                                     this.i18n('showPassword');
  }

  /**
   * Get the right icon to display when hiding/showing a password.
   */
  private getIconClass_(): string {
    assert(!this.isInFederatedViewMode_);
    return this.isPasswordVisible_ ? 'icon-visibility-off' : 'icon-visibility';
  }

  /**
   * Gets the initial text to show in the website input.
   */
  private getWebsite_(): string {
    if (this.dialogMode === PasswordDialogMode.ADD) {
      return '';
    }

    return this.existingEntry!.isAndroidCredential ?
        this.existingEntry!.urls.shown :
        this.existingEntry!.urls.link;
  }

  /**
   * Gets the initial text to show in the password input: the password for a
   * regular credential, the federated text for a federated credential or empty
   * string in the ADD mode.
   */
  private getPassword_(): string {
    switch (this.dialogMode) {
      case PasswordDialogMode.FEDERATED_VIEW:
        // FEDERATED_VIEW mode implies |existingEntry| is a federated
        // credential.
        return this.existingEntry!.federationText!;
      case PasswordDialogMode.EDIT:
        return this.existingEntry!.password || '';
      case PasswordDialogMode.ADD:
        return '';
      default:
        assertNotReached();
    }
  }

  /**
   * Handler for tapping the show/hide button.
   */
  private onShowPasswordButtonClick_() {
    assert(!this.isInFederatedViewMode_);
    this.isPasswordVisible_ = !this.isPasswordVisible_;
  }

  /**
   * Handler for tapping the 'done' or 'save' button depending on |dialogMode|.
   * For 'save' button it should save new password. After pressing action button
   * the edit dialog should be closed.
   */
  private onActionButtonClick_() {
    switch (this.dialogMode) {
      case PasswordDialogMode.FEDERATED_VIEW:
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
    if (!this.$.storePicker.hidden) {
      chrome.metricsPrivate.recordBoolean(
          'PasswordManager.AddCredentialFromSettings.AccountStoreUsed',
          useAccountStore);
    }
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.AddCredentialFromSettings.UserAction2',
        AddCredentialFromSettingsUserInteractions.CREDENTIAL_ADDED,
        AddCredentialFromSettingsUserInteractions.COUNT);
    PasswordManagerImpl.getInstance()
        .addPassword({
          url: this.$.websiteInput.value,
          username: this.username_,
          password: this.password_,
          note: this.note_.trim(),
          useAccountStore: useAccountStore,
        })
        .then(() => {
          if (this.isPasswordNotesEnabled_ && !!this.note_.trim()) {
            recordPasswordNoteAction(
                PasswordNoteAction.NOTE_ADDED_IN_ADD_DIALOG);
          }
        })
        .finally(() => {
          this.close();
        });
  }

  private changePassword_() {
    const params: chrome.passwordsPrivate.ChangeSavedPasswordParams = {
      username: this.username_,
      password: this.password_,
    };
    if (this.isPasswordNotesEnabled_) {
      params.note = this.note_;
    }

    PasswordManagerImpl.getInstance()
        .changeSavedPassword(this.existingEntry!.id, params)
        .then(newId => {
          if (this.isPasswordNotesEnabled_) {
            const newNote = this.note_.trim();
            const oldNote = this.existingEntry!.note === undefined ?
                '' :
                this.existingEntry!.note;
            if (oldNote === newNote) {
              recordPasswordNoteAction(PasswordNoteAction.NOTE_NOT_CHANGED);
            } else if (oldNote !== '' && newNote !== '') {
              recordPasswordNoteAction(
                  PasswordNoteAction.NOTE_EDITED_IN_EDIT_DIALOG);
            } else if (oldNote !== '') {
              recordPasswordNoteAction(
                  PasswordNoteAction.NOTE_REMOVED_IN_EDIT_DIALOG);
            } else {
              recordPasswordNoteAction(
                  PasswordNoteAction.NOTE_ADDED_IN_EDIT_DIALOG);
            }
          }
          if (this.isPasswordViewPageEnabled_) {
            const newEntry = {
              ...this.existingEntry!,
              username: this.username_,
              password: this.password_,
              note: this.note_.trim(),
              id: newId,
            };
            this.dispatchChangePasswordEvent_(newEntry);
          }
        })
        .finally(() => {
          this.close();
        });
  }

  private dispatchChangePasswordEvent_(
      newEntry: chrome.passwordsPrivate.PasswordUiEntry) {
    this.dispatchEvent(new CustomEvent(SAVED_PASSWORD_EDITED_EVENT_NAME, {
      bubbles: true,
      composed: true,
      detail: newEntry,
    }));
  }

  private getActionButtonName_(): string {
    return this.isInFederatedViewMode_ ? this.i18n('done') : this.i18n('save');
  }

  private onWebsiteInputBlur_() {
    if (!this.isWebsiteEditable_()) {
      // Manually de-select text when input is readonly.
      this.shadowRoot!.getSelection()!.removeAllRanges();
      return;
    }
    if (this.websiteUrls_ && !this.$.websiteInput.value.includes('.')) {
      this.websiteInputErrorMessage_ =
          this.i18n('missingTLD', `${this.$.websiteInput.value}.com`);
      this.websiteInputInvalid_ = true;
    }
  }

  /**
   * Gets the HTML-formatted message to indicate in which locations the password
   * is stored.
   */
  private getStorageDetailsMessage_(): string {
    if (this.dialogMode === PasswordDialogMode.ADD) {
      // Storage message is not shown in the ADD mode.
      return '';
    }
    switch (this.existingEntry!.storedIn) {
      case chrome.passwordsPrivate.PasswordStoreSet.DEVICE:
        return this.i18n('passwordStoredOnDevice');
      case chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT:
        return this.i18n('passwordStoredInAccount');
      case chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT:
        return this.i18n('passwordStoredInAccountAndOnDevice');
    }
  }

  private getStoreOptionAccountText_(): string {
    if (this.dialogMode !== PasswordDialogMode.ADD) {
      // Store picker is only shown in the ADD mode.
      return '';
    }

    return this.i18n('addPasswordStoreOptionAccount', this.accountEmail!);
  }

  private getTitle_(): string {
    switch (this.dialogMode) {
      case PasswordDialogMode.ADD:
        return this.i18n('addPasswordTitle');
      case PasswordDialogMode.EDIT:
        return this.i18n('editPasswordTitle');
      case PasswordDialogMode.FEDERATED_VIEW:
        return this.i18n('passwordDetailsTitle');
      default:
        assertNotReached();
    }
  }

  private shouldShowStorageDetails_(): boolean {
    return this.dialogMode !== PasswordDialogMode.ADD &&
        this.isAccountStoreUser;
  }

  private shouldShowStorePicker_(): boolean {
    return this.dialogMode === PasswordDialogMode.ADD &&
        this.isAccountStoreUser;
  }

  private isWebsiteEditable_(): boolean {
    return this.dialogMode === PasswordDialogMode.ADD;
  }

  private shouldAutofocusWebsiteInput_(): boolean {
    return this.dialogMode === PasswordDialogMode.ADD &&
        !this.isAccountStoreUser;
  }

  /**
   * @return The text to be displayed as the dialog's footnote.
   */
  private getFootnote_(): string {
    switch (this.dialogMode) {
      case PasswordDialogMode.ADD:
        return this.i18n('addPasswordFootnote');
      case PasswordDialogMode.EDIT:
      case PasswordDialogMode.FEDERATED_VIEW:
        return this.i18n(
            'editPasswordFootnote', this.existingEntry!.urls.shown);
      default:
        return '';
    }
  }

  private getLabelForWebsiteInput_(): string {
    if (this.dialogMode === PasswordDialogMode.ADD) {
      return this.i18n('editPasswordWebsiteLabel');
    }
    return this.i18n(
        this.existingEntry!.isAndroidCredential ? 'editPasswordAppLabel' :
                                                  'editPasswordWebsiteLabel');
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
    assert(this.dialogMode === PasswordDialogMode.ADD);
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

  private getUsernameErrorMessage_(): string {
    return this.websiteUrls_ ?
        this.i18n('usernameAlreadyUsed', this.websiteUrls_.shown) :
        '';
  }

  private getViewExistingPasswordAriaDescription_(): string {
    return this.websiteUrls_ ? this.i18n(
                                   'viewExistingPasswordAriaDescription',
                                   this.username_, this.websiteUrls_.shown) :
                               '';
  }

  private onViewExistingPasswordClick_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.AddCredentialFromSettings.UserAction2',
        AddCredentialFromSettingsUserInteractions.DUPLICATE_CREDENTIAL_VIEWED,
        AddCredentialFromSettingsUserInteractions.COUNT);
    const existingEntry = this.savedPasswords.find(entry => {
      return entry.urls.signonRealm === this.websiteUrls_!.signonRealm &&
          entry.username === this.username_;
    })!;
    if (this.isPasswordViewPageEnabled_) {
      // wrap the id in a PasswordListItemElement:
      this.dispatchEvent(
          new CustomEvent(PASSWORD_VIEW_PAGE_REQUESTED_EVENT_NAME, {
            bubbles: true,
            composed: true,
            detail: {entry: existingEntry} as unknown as
                PasswordListItemElement,
          }));
      this.close();
      return;
    }
    this.requestPlaintextPassword(
            existingEntry.id, chrome.passwordsPrivate.PlaintextReason.EDIT)
        .then(password => {
          existingEntry.password = password;
          this.switchToEditMode_(existingEntry);
        }, () => {});
  }

  private switchToEditMode_(existingEntry:
                                chrome.passwordsPrivate.PasswordUiEntry) {
    this.existingEntry = existingEntry;
    this.initDialog_();
    this.$.dialog.focus();
  }

  /**
   * Checks whether edited username is not used for the same website.
   */
  private computeUsernameInputInvalid_(): boolean {
    if (this.isInFederatedViewMode_ || !this.websiteUrls_ ||
        !this.usernamesByOrigin_) {
      return false;
    }
    if (this.dialogMode === PasswordDialogMode.EDIT &&
        this.username_ === this.existingEntry!.username) {
      // The value hasn't changed.
      return false;
    }
    // TODO(crbug.com/1264468): Consider moving duplication check to backend.
    const isDuplicate =
        this.usernamesByOrigin_.has(this.websiteUrls_.signonRealm) &&
        this.usernamesByOrigin_.get(this.websiteUrls_.signonRealm)!.has(
            this.username_);

    if (isDuplicate && this.dialogMode === PasswordDialogMode.ADD) {
      chrome.metricsPrivate.recordEnumerationValue(
          'PasswordManager.AddCredentialFromSettings.UserAction2',
          AddCredentialFromSettingsUserInteractions
              .DUPLICATED_CREDENTIAL_ENTERED,
          AddCredentialFromSettingsUserInteractions.COUNT);
    }

    return isDuplicate;
  }

  /**
   * Used for the fast check whether edited username is already used.
   */
  private getUsernamesByOrigin_(): Map<string, Set<string>> {
    assert(!this.isInFederatedViewMode_);
    const relevantPasswords = this.dialogMode === PasswordDialogMode.EDIT ?
        // In EDIT mode entries considered duplicates only if in the same store.
        this.savedPasswords.filter(item => {
          return item.storedIn === this.existingEntry!.storedIn;
        }) :
        // In ADD mode entries considered duplicates irrespective of the store.
        this.savedPasswords;

    // Group existing usernames by origin.
    return relevantPasswords.reduce(function(usernamesByOrigin, entry) {
      const origin = entry.urls.signonRealm;
      if (!usernamesByOrigin.has(origin)) {
        usernamesByOrigin.set(origin, new Set());
      }
      usernamesByOrigin.get(origin).add(entry.username);
      return usernamesByOrigin;
    }, new Map());
  }

  private extendAuthValidityOnUserInteraction_() {
    if (!this.isPasswordViewPageEnabled_) {
      return;
    }
    PasswordManagerImpl.getInstance().extendAuthValidity();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-edit-dialog': PasswordEditDialogElement;
  }
}

customElements.define(PasswordEditDialogElement.is, PasswordEditDialogElement);
