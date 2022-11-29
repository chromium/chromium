// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-import-dialog' is the dialog that allows importing
 * passwords.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared.css.js';
import '../site_favicon.js';
import './passwords_shared.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {getTemplate} from './passwords_import_dialog.html.js';

export interface PasswordsImportDialogElement {
  $: {
    dialog: CrDialogElement,
    descriptionText: HTMLElement,
    successTip: HTMLElement,
    failuresSummary: HTMLElement,
    storePicker: HTMLSelectElement,
    chooseFile: CrButtonElement,
    close: CrButtonElement,
    expandButton: HTMLElement,
  };
}

const PasswordsImportDialogElementBase = I18nMixin(PolymerElement);

export const IMPORT_HELP_LANDING_PAGE: string =
    'https://support.google.com/chrome/?p=import-passwords-desktop';

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

export function recordPasswordsImportInteraction(
    interaction: PasswordsImportDesktopInteractions) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.Import.DesktopInteractions', interaction,
      PasswordsImportDesktopInteractions.COUNT);
}

/**
 * Should be kept in sync with
 * |password_manager::metrics_util::PasswordsImportDesktopInteractions|.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum PasswordsImportDesktopInteractions {
  DIALOG_OPENED_FROM_THREE_DOT_MENU = 0,
  DIALOG_OPENED_FROM_EMPTY_STATE = 1,
  CANCELED_BEFORE_FILE_SELECT = 2,
  // Must be last.
  COUNT = 3,
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

      inProgress_: {
        type: Boolean,
        value: false,
      },

      results_: Object,
      failedImportsWithKnownErrors_: Array,

      rowsWithUnknownErrorsSummary_: String,

      showRowsWithUnknownErrorsSummary_: {
        type: Boolean,
        value: false,
      },
    };
  }

  dialogState: ImportDialogState;
  isUserSyncingPasswords: boolean;
  isAccountStoreUser: boolean;
  accountEmail: string;
  private results_: chrome.passwordsPrivate.ImportResults|null;
  private failedImportsWithKnownErrors_: chrome.passwordsPrivate.ImportEntry[];
  private rowsWithUnknownErrorsSummary_: string;
  private showRowsWithUnknownErrorsSummary_: boolean;
  // Refers both to syncing users with sync enabled for passwords and account
  // store users who choose to import passwords to their account.
  private passwordsSavedToAccount_: boolean;
  private inProgress_: boolean;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    if (this.isAccountStoreUser) {
      this.$.descriptionText.textContent =
          this.i18n('importPasswordsGenericDescription');
      PasswordManagerImpl.getInstance().isAccountStoreDefault().then(
          isAccountStoreDefault => {
            this.passwordsSavedToAccount_ = isAccountStoreDefault;
            this.$.storePicker.value = isAccountStoreDefault ?
                StoreOption.ACCOUNT :
                StoreOption.DEVICE;
          });
    } else if (this.isUserSyncingPasswords) {
      this.passwordsSavedToAccount_ = true;
      this.$.descriptionText.textContent =
          this.i18n('importPasswordsDescriptionAccount', this.accountEmail);
    } else {
      this.passwordsSavedToAccount_ = false;
      this.$.descriptionText.textContent =
          this.i18n('importPasswordsDescriptionDevice');
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

  private isTipBoxHidden_(): boolean {
    // Tip box is only shown in "success" state if all passwords were imported.
    return !this.isState_(ImportDialogState.SUCCESS) ||
        (this.isState_(ImportDialogState.SUCCESS) &&
         !!this.results_!.failedImports.length);
  }

  private isFailuresSummaryHidden_(): boolean {
    return !this.isState_(ImportDialogState.SUCCESS) ||
        (this.isState_(ImportDialogState.SUCCESS) &&
         !this.results_!.failedImports.length);
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
    switch (this.results_.status) {
      case chrome.passwordsPrivate.ImportResultsStatus.SUCCESS:
        this.handleSuccess_();
        return;
      case chrome.passwordsPrivate.ImportResultsStatus.MAX_FILE_SIZE:
        this.$.descriptionText.textContent =
            this.i18n('importPasswordsFileSizeExceeded');
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.IO_ERROR:
      case chrome.passwordsPrivate.ImportResultsStatus.UNKNOWN_ERROR:
        this.$.descriptionText.textContent =
            this.i18n('importPasswordsUnknownError');
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.NUM_PASSWORDS_EXCEEDED:
        this.$.descriptionText.textContent =
            this.i18n('importPasswordsLimitExceeded', 3000);
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.BAD_FORMAT:
        this.$.descriptionText.innerHTML = this.i18nAdvanced(
            'importPasswordsBadFormatError',
            {substitutions: [IMPORT_HELP_LANDING_PAGE]});
        this.$.descriptionText.querySelector('b')!.textContent =
            this.results_.fileName;
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.DISMISSED:
        // Dialog state should not change if a system file picker was dismissed.
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.IMPORT_ALREADY_ACTIVE:
        this.$.descriptionText.textContent =
            this.i18n('importPasswordsAlreadyActive');
        this.dialogState = ImportDialogState.ALREADY_ACTIVE;
        break;
      default:
        assertNotReached();
    }
    this.$.close.focus();
  }

  private async handleSuccess_() {
    assert(this.results_);
    if (!this.results_.failedImports.length) {
      this.setSuccessTip_();
    } else {
      const rowsWithUnknownErrorCount =
          this.results_.failedImports
              .filter(
                  (entry) => entry.status ===
                      chrome.passwordsPrivate.ImportEntryStatus.UNKNOWN_ERROR)
              .length;
      this.failedImportsWithKnownErrors_ = this.results_.failedImports.filter(
          (entry) => entry.status !==
              chrome.passwordsPrivate.ImportEntryStatus.UNKNOWN_ERROR);
      if (rowsWithUnknownErrorCount) {
        this.rowsWithUnknownErrorsSummary_ =
            await PluralStringProxyImpl.getInstance().getPluralString(
                'importPasswordsBadRowsFormat', rowsWithUnknownErrorCount);
        this.showRowsWithUnknownErrorsSummary_ = true;
      }
    }
    if (this.passwordsSavedToAccount_) {
      const descriptionText =
          await PluralStringProxyImpl.getInstance().getPluralString(
              'importPasswordsSuccessSummaryAccount',
              this.results_.numberImported);
      this.$.descriptionText.textContent =
          descriptionText.replace('$1', this.accountEmail);
    } else {
      this.$.descriptionText.textContent =
          await PluralStringProxyImpl.getInstance().getPluralString(
              'importPasswordsSuccessSummaryDevice',
              this.results_.numberImported);
    }
    this.dialogState = ImportDialogState.SUCCESS;

    if (this.isFailuresSummaryHidden_()) {
      this.$.close.focus();
    } else {
      this.$.expandButton.focus();
    }
  }

  private getStoreOptionAccountText_(): string {
    return this.i18n('addPasswordStoreOptionAccount', this.accountEmail!);
  }

  private setSuccessTip_() {
    this.$.successTip.innerHTML =
        this.i18nAdvanced('importPasswordsSuccessTip');
    this.$.successTip.querySelector('b')!.textContent = this.results_!.fileName;
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
    switch (status) {
      case chrome.passwordsPrivate.ImportEntryStatus.MISSING_PASSWORD:
        return this.i18n('importPasswordsMissingPassword');
      case chrome.passwordsPrivate.ImportEntryStatus.MISSING_URL:
        return this.i18n('importPasswordsMissingURL');
      case chrome.passwordsPrivate.ImportEntryStatus.INVALID_URL:
        return this.i18n('importPasswordsInvalidURL');
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_URL:
        return this.i18n('importPasswordsLongURL');
      case chrome.passwordsPrivate.ImportEntryStatus.NON_ASCII_URL:
        return this.i18n('importPasswordsNonASCIIURL');
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
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_NOTE:
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_CONCATENATED_NOTE:
        return this.i18n('importPasswordsLongNote');
      case chrome.passwordsPrivate.ImportEntryStatus.UNKNOWN_ERROR:
      default:
        assertNotReached();
    }
  }
  private getCloseButtonText_(): string {
    switch (this.dialogState) {
      case ImportDialogState.START:
        return this.i18n('cancel');
      case ImportDialogState.ERROR:
      case ImportDialogState.ALREADY_ACTIVE:
        return this.i18n('close');
      case ImportDialogState.SUCCESS:
        return this.i18n('done');
      default:
        assertNotReached();
    }
  }

  private getCloseButtonType_(): string {
    switch (this.dialogState) {
      case ImportDialogState.START:
      case ImportDialogState.ERROR:
        return 'cancel';
      case ImportDialogState.ALREADY_ACTIVE:
      case ImportDialogState.SUCCESS:
        return 'action';
      default:
        assertNotReached();
    }
  }

  private onCloseClick_() {
    if (this.isState_(ImportDialogState.START)) {
      recordPasswordsImportInteraction(
          PasswordsImportDesktopInteractions.CANCELED_BEFORE_FILE_SELECT);
    }
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
