// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-import-dialog' is the dialog that allows importing
 * passwords.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {getTemplate} from './passwords_import_dialog.html.js';

export interface PasswordsImportDialogElement {
  $: {
    dialog: CrDialogElement,
    descriptionText: HTMLElement,
    storePicker: HTMLSelectElement,
  };
}

const PasswordsImportDialogElementBase = I18nMixin(PolymerElement);

export enum ImportDialogState {
  START,
  ERROR,
  SUCCESS,
  ALREADY_ACTIVE,
}

enum StoreOption {
  ACCOUNT = 'account',
  DEVICE = 'device',
}

export class PasswordsImportDialogElement extends
    PasswordsImportDialogElementBase {
  static get is() {
    return 'passwords-import-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dialogState: Number,

      importDialogStateEnum_: {
        type: Object,
        value: ImportDialogState,
        readOnly: true,
      },

      storeOptionEnum_: {
        type: Object,
        value: StoreOption,
        readOnly: true,
      },

      descriptionText_: String,

      inProgress_: {
        type: Boolean,
        value: false,
      },

      results_: Object,
    };
  }

  dialogState: ImportDialogState;
  isUserSyncingPasswords: boolean;
  isAccountStoreUser: boolean;
  accountEmail: string;
  private results_: chrome.passwordsPrivate.ImportResults|null;
  // Refers both to syncing users with sync enabled for passwords and account
  // store users who choose to import passwords to their account.
  private passwordsSavedToAccount_: boolean;
  private descriptionText_: string;
  private inProgress_: boolean;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    if (this.isAccountStoreUser) {
      this.descriptionText_ = this.i18n('importPasswordsGenericDescription');
      PasswordManagerImpl.getInstance().isAccountStoreDefault().then(
          isAccountStoreDefault => {
            this.passwordsSavedToAccount_ = isAccountStoreDefault;
            this.$.storePicker.value = isAccountStoreDefault ?
                StoreOption.ACCOUNT :
                StoreOption.DEVICE;
          });
    } else if (this.isUserSyncingPasswords) {
      this.passwordsSavedToAccount_ = true;
      this.descriptionText_ =
          this.i18n('importPasswordsDescriptionAccount', this.accountEmail);
    } else {
      this.passwordsSavedToAccount_ = false;
      this.descriptionText_ = this.i18n('importPasswordsDescriptionDevice');
    }
    this.dialogState = ImportDialogState.START;
  }

  private isState_(state: ImportDialogState): boolean {
    return this.dialogState === state;
  }

  private showChooseFileButton_(): boolean {
    return this.isState_(ImportDialogState.START) ||
        this.isState_(ImportDialogState.ERROR);
  }

  private showTipBox_(): boolean {
    // Tip box is only shown in "success" state if all passwords were imported.
    return this.isState_(ImportDialogState.SUCCESS) &&
        !this.results_!.failedImports.length;
  }

  private showFailuresSummary_(): boolean {
    return this.isState_(ImportDialogState.SUCCESS) &&
        !!this.results_!.failedImports.length;
  }

  private shouldShowStorePicker_(): boolean {
    return this.isState_(ImportDialogState.START) && this.isAccountStoreUser;
  }

  /**
   * Handler for clicking the 'chooseFile' button. It triggers import flow.
   */
  private async onChooseFileClick_() {
    this.inProgress_ = true;
    // For "non-account-store-users" users passwords are stored in the "profile"
    // (DEVICE) store.
    let destinationStore = chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
    if (this.isAccountStoreUser) {
      this.passwordsSavedToAccount_ =
          this.$.storePicker.value === StoreOption.ACCOUNT;
      if (this.passwordsSavedToAccount_) {
        destinationStore = chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
      }
    }
    this.results_ =
        await this.passwordManager_.importPasswords(destinationStore);
    this.inProgress_ = false;
    // TODO(crbug/1325290): set appropriate string for MAX_FILE_SIZE.
    switch (this.results_.status) {
      case chrome.passwordsPrivate.ImportResultsStatus.SUCCESS:
        this.handleSuccess_();
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.IO_ERROR:
      case chrome.passwordsPrivate.ImportResultsStatus.MAX_FILE_SIZE:
      case chrome.passwordsPrivate.ImportResultsStatus.UNKNOWN_ERROR:
        this.descriptionText_ = this.i18n('importPasswordsUnknownError');
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.NUM_PASSWORDS_EXCEEDED:
        this.descriptionText_ = this.i18n('importPasswordsLimitExceeded', 3000);
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.BAD_FORMAT:
        this.descriptionText_ =
            this.i18n('importPasswordsBadFormatError', this.results_.fileName);
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.DISMISSED:
        // Dialog state should not change if a system file picker was dismissed.
        return;
      case chrome.passwordsPrivate.ImportResultsStatus.IMPORT_ALREADY_ACTIVE:
        this.descriptionText_ = this.i18n('importPasswordsAlreadyActive');
        this.dialogState = ImportDialogState.ALREADY_ACTIVE;
        break;
      default:
        assertNotReached();
    }
  }

  private async handleSuccess_() {
    assert(this.results_);
    if (this.passwordsSavedToAccount_) {
      const descriptionText =
          await PluralStringProxyImpl.getInstance().getPluralString(
              'importPasswordsSuccessSummaryAccount',
              this.results_.numberImported);
      this.descriptionText_ = descriptionText.replace('$1', this.accountEmail);
    } else {
      this.descriptionText_ =
          await PluralStringProxyImpl.getInstance().getPluralString(
              'importPasswordsSuccessSummaryDevice',
              this.results_.numberImported);
    }
    this.dialogState = ImportDialogState.SUCCESS;
  }

  private getStoreOptionAccountText_(): string {
    return this.i18n('addPasswordStoreOptionAccount', this.accountEmail!);
  }

  private getSuccessTip_(): string {
    return this.i18n('importPasswordsSuccessTip', this.results_!.fileName);
  }

  private getFailedImportsSummary_(): string {
    return this.i18n(
        'importPasswordsFailuresSummary', this.results_!.failedImports.length);
  }

  private getFailedEntryTextDelimiter_(
      entry: chrome.passwordsPrivate.ImportEntry): string {
    if (entry.url && entry.username) {
      return ' • ';
    }
    return '';
  }

  private getFailedEntryError_(
      status: chrome.passwordsPrivate.ImportEntryStatus): string {
    // TODO(crbug/1325290): return appropriate strings for LONG_URL,
    // NON_ASCII_URL, UNKNOWN_ERROR.
    switch (status) {
      case chrome.passwordsPrivate.ImportEntryStatus.MISSING_PASSWORD:
        return this.i18n('importPasswordsMissingPassword');
      case chrome.passwordsPrivate.ImportEntryStatus.MISSING_URL:
        return this.i18n('importPasswordsMissingURL');
      case chrome.passwordsPrivate.ImportEntryStatus.INVALID_URL:
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_URL:
      case chrome.passwordsPrivate.ImportEntryStatus.NON_ASCII_URL:
        return this.i18n('importPasswordsInvalidURL');
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_PASSWORD:
        return this.i18n('importPasswordsLongPassword');
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_USERNAME:
        return this.i18n('importPasswordsLongUsername');
      case chrome.passwordsPrivate.ImportEntryStatus.CONFLICT_PROFILE:
        if (!this.isAccountStoreUser && this.isUserSyncingPasswords) {
          return this.i18n('importPasswordsConflictAccount', this.accountEmail);
        }
        return this.i18n('importPasswordsConflictDevice');
      case chrome.passwordsPrivate.ImportEntryStatus.CONFLICT_ACCOUNT:
        return this.i18n('importPasswordsConflictAccount', this.accountEmail);
      case chrome.passwordsPrivate.ImportEntryStatus.UNKNOWN_ERROR:
        return '';
      default:
        assertNotReached();
    }
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onCloseClick_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-import-dialog': PasswordsImportDialogElement;
  }
}

customElements.define(
    PasswordsImportDialogElement.is, PasswordsImportDialogElement);
