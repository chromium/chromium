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
import './password_preview_item.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {getTemplate} from './passwords_import_dialog.html.js';

export interface PasswordsImportDialogElement {
  $: {
    dialog: CrDialogElement,
    dialogTitle: HTMLElement,
    conflictsList: HTMLElement,
    descriptionText: HTMLElement,
    successTip: HTMLElement,
    failuresSummary: HTMLElement,
    storePicker: HTMLSelectElement,
    chooseFile: CrButtonElement,
    close: CrButtonElement,
    replace: CrButtonElement,
    skip: CrButtonElement,
    deleteFileOption: CrCheckboxElement,
  };
}

const PasswordsImportDialogElementBase = I18nMixin(PolymerElement);

export const IMPORT_HELP_LANDING_PAGE: string =
    'https://support.google.com/chrome/?p=import-passwords-desktop';

export enum ImportDialogState {
  START,
  CONFLICTS,
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
  UPM_STORE_PICKER_OPENED = 3,
  UPM_FILE_SELECT_LAUNCHED = 4,
  UPM_VIEW_PASSWORDS_CLICKED = 5,
  CONFLICTS_CANCELED = 6,
  CONFLICTS_REAUTH_FAILED = 7,
  CONFLICTS_SKIP_CLICKED = 8,
  CONFLICTS_REPLACE_CLICKED = 9,
  // Must be last.
  COUNT = 10,
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
      dialogState: {
        type: Number,
        observer: 'focusOnFirstActionableItem_',
      },

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

      enablePasswordsImportM2_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePasswordsImportM2');
        },
      },

      descriptionText_: String,
      results_: Object,
      failedImportsWithKnownErrors_: Array,
      failedImportsSummary_: String,

      rowsWithUnknownErrorsSummary_: String,

      showRowsWithUnknownErrorsSummary_: {
        type: Boolean,
        value: false,
      },

      conflictsSelectedForReplace_: {
        type: Array,
        value: [],
      },

      shouldDisableReplaceButton_: {
        type: Boolean,
        computed: 'computeShouldDisableReplaceButton_(' +
            'conflictsSelectedForReplace_, inProgress_)',
      },
    };
  }

  dialogState: ImportDialogState;
  isUserSyncingPasswords: boolean;
  isAccountStoreUser: boolean;
  accountEmail: string;
  private results_: chrome.passwordsPrivate.ImportResults|null;
  private descriptionText_: TrustedHTML;
  private failedImportsWithKnownErrors_: chrome.passwordsPrivate.ImportEntry[];
  private conflicts_: chrome.passwordsPrivate.ImportEntry[];
  private shouldDisableReplaceButton_: boolean;
  private conflictsSelectedForReplace_: number[];
  private failedImportsSummary_: string;
  private conflictsTitle_: string;
  private enablePasswordsImportM2_: boolean;
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
      this.descriptionText_ =
          this.i18nAdvanced('importPasswordsGenericDescription');
      PasswordManagerImpl.getInstance().isAccountStoreDefault().then(
          isAccountStoreDefault => {
            this.passwordsSavedToAccount_ = isAccountStoreDefault;
            this.$.storePicker.value = isAccountStoreDefault ?
                StoreOption.ACCOUNT :
                StoreOption.DEVICE;
          });
    } else if (this.isUserSyncingPasswords) {
      this.passwordsSavedToAccount_ = true;
      this.descriptionText_ = this.i18nAdvanced(
          'importPasswordsDescriptionAccount',
          {substitutions: [this.accountEmail]});
    } else {
      this.passwordsSavedToAccount_ = false;
      this.descriptionText_ =
          this.i18nAdvanced('importPasswordsDescriptionDevice');
    }
    this.dialogState = ImportDialogState.START;
  }

  private focusOnFirstActionableItem_() {
    afterNextRender(this, () => {
      let elementToFocus = this.$.close as HTMLElement;
      if (this.isState_(ImportDialogState.CONFLICTS)) {
        const firstCheckbox =
            this.$.conflictsList.querySelector('cr-checkbox') as HTMLElement;
        if (firstCheckbox) {
          elementToFocus = firstCheckbox;
        }
      } else if (!this.shouldHideDeleteFileOption_()) {
        elementToFocus = this.$.deleteFileOption as HTMLElement;
      } else if (this.shouldShowStorePicker_()) {
        elementToFocus = this.$.storePicker as HTMLElement;
      }
      focusWithoutInk(elementToFocus);
    });
  }

  private computeConflictsListClass_(): string {
    return this.inProgress_ ? 'disabled-conflicts-list' : '';
  }

  private isState_(state: ImportDialogState): boolean {
    return this.dialogState === state;
  }

  private showChooseFileButton_(): boolean {
    return this.isState_(ImportDialogState.START) ||
        this.isState_(ImportDialogState.ERROR);
  }

  private shouldHideTipBox_(): boolean {
    // Tip box is only shown in "success" state if all passwords were imported.
    // Only shown in Passwords Import M1.
    if (this.enablePasswordsImportM2_) {
      return true;
    }
    if (!this.isState_(ImportDialogState.SUCCESS)) {
      return true;
    }
    assert(this.results_);
    return !!this.results_.displayedEntries.length;
  }

  private shouldHideDeleteFileOption_(): boolean {
    // "Delete file" checkbox is only shown in "success" state if all passwords
    // were imported.
    if (!this.enablePasswordsImportM2_) {
      return true;
    }
    if (!this.isState_(ImportDialogState.SUCCESS)) {
      return true;
    }
    assert(this.results_);
    return !!this.results_.displayedEntries.length;
  }

  private shouldHideFailuresSummary_(): boolean {
    if (!this.isState_(ImportDialogState.SUCCESS)) {
      return true;
    }
    assert(this.results_);
    return !this.results_.displayedEntries.length;
  }

  private shouldShowStorePicker_(): boolean {
    return this.isState_(ImportDialogState.START) && this.isAccountStoreUser;
  }

  private getSelectedIds_(): number[] {
    const checkboxes = this.$.conflictsList.querySelectorAll('cr-checkbox');
    const selectedPasswords: number[] = [];
    checkboxes.forEach((checkbox: CrCheckboxElement) => {
      if (checkbox.checked) {
        selectedPasswords.push(Number(checkbox.dataset['id']));
      }
    });
    return selectedPasswords;
  }

  /**
   * Handler for ticking conflicting password checkbox.
   */
  private onPasswordSelectedChange_(): void {
    this.conflictsSelectedForReplace_ = this.getSelectedIds_();
  }

  private computeShouldDisableReplaceButton_(): boolean {
    return this.inProgress_ || !this.conflictsSelectedForReplace_.length;
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
    this.processResults_();
  }

  private async onSkipClick_() {
    assert(this.isState_(ImportDialogState.CONFLICTS));
    recordPasswordsImportInteraction(
        PasswordsImportDesktopInteractions.CONFLICTS_SKIP_CLICKED);
    this.inProgress_ = true;
    this.results_ =
        await this.passwordManager_.continueImport(/*selectedIds=*/[]);
    this.processResults_();
  }

  private async onReplaceClick_() {
    assert(this.isState_(ImportDialogState.CONFLICTS));
    recordPasswordsImportInteraction(
        PasswordsImportDesktopInteractions.CONFLICTS_REPLACE_CLICKED);
    this.inProgress_ = true;
    this.results_ = await this.passwordManager_.continueImport(
        this.conflictsSelectedForReplace_);
    this.processResults_();
  }

  private async processResults_() {
    assert(this.results_);
    this.inProgress_ = false;
    switch (this.results_.status) {
      case chrome.passwordsPrivate.ImportResultsStatus.SUCCESS:
        this.handleSuccess_();
        return;
      case chrome.passwordsPrivate.ImportResultsStatus.CONFLICTS:
        this.descriptionText_ =
            this.i18nAdvanced('importPasswordsConflictsDescription', {
              substitutions: [this.i18n('localPasswordManager')],
            });
        this.conflictsTitle_ =
            await PluralStringProxyImpl.getInstance().getPluralString(
                'importPasswordsConflictsTitle',
                this.results_.displayedEntries.length);
        this.conflicts_ = this.results_.displayedEntries;
        this.dialogState = ImportDialogState.CONFLICTS;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.MAX_FILE_SIZE:
        this.descriptionText_ =
            this.i18nAdvanced('importPasswordsFileSizeExceeded');
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.IO_ERROR:
      case chrome.passwordsPrivate.ImportResultsStatus.UNKNOWN_ERROR:
        this.descriptionText_ =
            this.i18nAdvanced('importPasswordsUnknownError');
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.NUM_PASSWORDS_EXCEEDED:
        this.descriptionText_ = this.i18nAdvanced(
            'importPasswordsLimitExceeded', {substitutions: ['3000']});
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.BAD_FORMAT:
        this.descriptionText_ =
            this.i18nAdvanced('importPasswordsBadFormatError', {
              attrs: ['class'],
              substitutions: [this.results_.fileName, IMPORT_HELP_LANDING_PAGE],
            });
        this.dialogState = ImportDialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.DISMISSED:
        if (this.isState_(ImportDialogState.CONFLICTS)) {
          recordPasswordsImportInteraction(
              PasswordsImportDesktopInteractions.CONFLICTS_REAUTH_FAILED);
        }
        // Dialog state should not change if a system file picker was dismissed.
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.IMPORT_ALREADY_ACTIVE:
        this.descriptionText_ =
            this.i18nAdvanced('importPasswordsAlreadyActive');
        this.dialogState = ImportDialogState.ALREADY_ACTIVE;
        break;
      default:
        assertNotReached();
    }
  }

  private async handleSuccess_() {
    assert(this.results_);
    if (this.results_.displayedEntries.length) {
      const rowsWithUnknownErrorCount =
          this.results_.displayedEntries
              .filter(
                  (entry) => entry.status ===
                      chrome.passwordsPrivate.ImportEntryStatus.UNKNOWN_ERROR)
              .length;
      this.failedImportsWithKnownErrors_ =
          this.results_.displayedEntries.filter(
              (entry) => entry.status !==
                  chrome.passwordsPrivate.ImportEntryStatus.UNKNOWN_ERROR);
      this.failedImportsSummary_ =
          await PluralStringProxyImpl.getInstance().getPluralString(
              'importPasswordsFailuresSummary',
              this.results_.displayedEntries.length);
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
      this.descriptionText_ =
          sanitizeInnerHtml(descriptionText.replace('$1', this.accountEmail));
    } else {
      const descriptionText =
          await PluralStringProxyImpl.getInstance().getPluralString(
              'importPasswordsSuccessSummaryDevice',
              this.results_.numberImported);
      this.descriptionText_ = sanitizeInnerHtml(descriptionText);
    }
    this.dialogState = ImportDialogState.SUCCESS;
  }

  private getStoreOptionAccountText_(): string {
    return this.i18n('addPasswordStoreOptionAccount', this.accountEmail!);
  }

  private getSuccessTipHtml_(): TrustedHTML {
    assert(this.results_);
    return this.i18nAdvanced(
        'importPasswordsSuccessTip',
        {attrs: ['class'], substitutions: [this.results_.fileName]});
  }

  private getCheckboxLabelHtml_(): TrustedHTML {
    assert(this.results_);
    return this.i18nAdvanced(
        'importPasswordsDeleteFileOption',
        {attrs: ['class'], substitutions: [this.results_.fileName]});
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

  private getDialogTitleText_(): string {
    switch (this.dialogState) {
      case ImportDialogState.START:
      case ImportDialogState.ALREADY_ACTIVE:
        return this.i18n('importPasswordsTitle');
      case ImportDialogState.CONFLICTS:
        return this.conflictsTitle_;
      case ImportDialogState.ERROR:
        return this.i18n('importPasswordsErrorTitle');
      case ImportDialogState.SUCCESS:
        assert(this.results_);
        if (!this.results_.displayedEntries.length) {
          return this.i18n('importPasswordsSuccessTitle');
        }
        return this.i18n('importPasswordsCompleteTitle');
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
      case ImportDialogState.CONFLICTS:
        return this.i18n('importPasswordsCancel');
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
      case ImportDialogState.CONFLICTS:
        return 'flex-float-left cancel';
      case ImportDialogState.ALREADY_ACTIVE:
      case ImportDialogState.SUCCESS:
        return 'action';
      default:
        assertNotReached();
    }
  }

  private async onCloseClick_() {
    if (this.isState_(ImportDialogState.START)) {
      recordPasswordsImportInteraction(
          PasswordsImportDesktopInteractions.CANCELED_BEFORE_FILE_SELECT);
    }
    if (this.isState_(ImportDialogState.CONFLICTS)) {
      recordPasswordsImportInteraction(
          PasswordsImportDesktopInteractions.CONFLICTS_CANCELED);
    }
    if (this.enablePasswordsImportM2_) {
      // Trigger the file deletion if checkbox is ticked in SUCCESS (with no
      // errors) state.
      const deleteFile = !this.shouldHideDeleteFileOption_() &&
          this.$.deleteFileOption.checked;
      await this.passwordManager_.resetImporter(deleteFile);
      if (!this.shouldHideDeleteFileOption_) {
        chrome.metricsPrivate.recordBoolean(
            'PasswordManager.Import.FileDeletionSelected', deleteFile);
      }
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
