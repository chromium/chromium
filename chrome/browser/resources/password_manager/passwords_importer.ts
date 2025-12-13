// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-importer' is a component for importing passwords
 * from a .csv file. It is a state machine that transitions through several
 * states, managed by the `dialogState_` property.
 *
 * The user initiates the import process, but the initial interaction differs
 * based on their account type:
 *
 * - For "Account Store" users: The user
 *   clicks on the entire "Import passwords" row. This action transitions the
 *   state to `STORE_PICKER`, where they can choose to save passwords to their
 *   account or the local device. After making a selection, they click the
 *   "Select file" button to continue.
 * - For other users: A "Select file" button is displayed directly on the
 *   row. Clicking this button bypasses the store picker and immediately
 *   transitions the state to `IN_PROGRESS`, starting the file selection
 *   process.
 *
 * The rest of the flow is as follows:
 *
 * 1.  File Processing (`IN_PROGRESS`): The user is prompted to select a
 *     `.csv` file. While the file is being read and processed, a spinner is
 *     shown.
 *
 * 2.  Resolution: After processing, the machine transitions to one of the
 *     following terminal states:
 *     - `SUCCESS`: The passwords were imported successfully.
 *     - `CONFLICTS`: The file contained passwords that already exist. The
 *       user is prompted to select which ones to overwrite.
 *     - `ERROR`: An error occurred (e.g., bad file format, file too
 *       large).
 *     - `ALREADY_ACTIVE`: Another import process was already in progress in
 *       another window.
 *
 * 3.  Completion (`NO_DIALOG`): From any of the resolution states, closing
 *     the dialog resets the state machine back to `NO_DIALOG`.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_spinner_style.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import './site_favicon.js';
import './dialogs/password_preview_item.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {PasswordManagerProxy} from './password_manager_proxy.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './passwords_importer.html.js';
import {Page, Router} from './router.js';

export interface PasswordsImporterElement {
  $: {
    linkRow: CrLinkRowElement,
    selectFileButtonLinkRow: CrButtonElement,
  };
}

/**
 * The states of the importer. See the file-level comment for more details on
 * the state machine.
 *
 *                 +------------------+
 *                 |    NO_DIALOG     |
 *                 +------------------+
 *                        |
 *                        v
 *                 +------------------+
 *                 |   STORE_PICKER   | (If isAccountStoreUser)
 *                 +------------------+
 *                        |
 *                        v (file selection)
 *                 +------------------+
 *                 |   IN_PROGRESS    |
 *                 +------------------+
 *                        |
 *     +------------------+------------------+------------------+
 *     |                  |                  |                  |
 *     v                  v                  v                  v
 * +-------+      +-----------+      +-----------+      +----------------+
 * | ERROR |      | CONFLICTS |      |  SUCCESS  |      | ALREADY_ACTIVE |
 * +-------+      +-----------+      +-----------+      +----------------+
 *     |                  |                  |                  |
 *     +------------------+------------------+------------------+
 *                        |
 *                        v
 *                 +------------------+
 *                 |    NO_DIALOG     |
 *                 +------------------+
 */
enum DialogState {
  NO_DIALOG,
  IN_PROGRESS,
  STORE_PICKER,
  SUCCESS,
  ERROR,
  ALREADY_ACTIVE,
  CONFLICTS,
}

/**
 * Should be kept in sync with PasswordsImportDesktopInteractions in enums.xml.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
enum PasswordsImportDesktopInteractions {
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

function recordPasswordsImportInteraction(
    interaction: PasswordsImportDesktopInteractions) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.Import.DesktopInteractions', interaction,
      PasswordsImportDesktopInteractions.COUNT);
}

const PasswordsImporterElementBase = I18nMixin(PolymerElement);

export class PasswordsImporterElement extends PasswordsImporterElementBase {
  static get is() {
    return 'passwords-importer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isUserSyncingPasswords: {
        type: Boolean,
        value: false,
      },

      isAccountStoreUser: {
        type: Boolean,
        value: false,
      },

      accountEmail: String,

      dialogState_: {
        type: Number,
        value: DialogState.NO_DIALOG,
      },

      dialogStateEnum_: {
        type: Object,
        value: DialogState,
        readOnly: true,
      },

      storeOptionEnum_: {
        type: Object,
        value: chrome.passwordsPrivate.PasswordStoreSet,
        readOnly: true,
      },

      selectedStoreOption_: String,

      results_: {
        type: Object,
        value: null,
      },

      successDescription_: String,
      failedImportsSummary_: String,
      rowsWithUnknownErrorsSummary_: String,

      conflictsDialogTitle_: String,

      conflicts_: {
        type: Array,
        value: [],
      },

      conflictsSelectedForReplace_: {
        type: Array,
        value: [],
      },

      showRowsWithUnknownErrorsSummary_: {
        type: Boolean,
        value: false,
      },

      bannerDescription_: {
        type: String,
        computed: 'computeBannerDescription_(isUserSyncingPasswords,' +
            'isAccountStoreUser, accountEmail)',
      },
    };
  }

  static get observers() {
    return [
      'updateDefaultStore_(isAccountStoreUser)',
      'updatePasswordsSavedToAccount_(isUserSyncingPasswords)',
    ];
  }

  declare isUserSyncingPasswords: boolean;
  declare isAccountStoreUser: boolean;
  declare accountEmail: string;

  declare private dialogState_: DialogState;
  // Refers both to syncing users with sync enabled for passwords and account
  // store users who choose to import passwords to their account.
  private passwordsSavedToAccount_: boolean;
  declare private selectedStoreOption_: string;
  declare private bannerDescription_: string;
  declare private results_: chrome.passwordsPrivate.ImportResults|null;
  declare private conflicts_: chrome.passwordsPrivate.ImportEntry[];
  declare private conflictsSelectedForReplace_: number[];
  declare private successDescription_: string;
  declare private conflictsDialogTitle_: string;
  declare private failedImportsSummary_: string;
  declare private rowsWithUnknownErrorsSummary_: string;
  declare private showRowsWithUnknownErrorsSummary_: boolean;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  launchImport() {
    recordPasswordsImportInteraction(
        PasswordsImportDesktopInteractions.DIALOG_OPENED_FROM_EMPTY_STATE);
    this.dialogState_ = DialogState.IN_PROGRESS;
    // Timeout is needed to allow Polymer to render the Settings page before the
    // system file picker has been opened.
    setTimeout(() => {
      if (this.isAccountStoreUser) {
        this.dialogState_ = DialogState.STORE_PICKER;
      } else {
        this.selectFileHelper_();
      }
    }, 200);
  }

  private updateDefaultStore_() {
    if (this.isAccountStoreUser) {
      this.selectedStoreOption_ =
          chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
    }
  }

  private updatePasswordsSavedToAccount_() {
    this.passwordsSavedToAccount_ = this.isUserSyncingPasswords;
  }

  private isState_(state: DialogState): boolean {
    return this.dialogState_ === state;
  }

  private computeBannerDescription_(): string {
    if (this.isAccountStoreUser) {
      return this.i18n('importPasswordsGenericDescription');
    }
    if (this.isUserSyncingPasswords) {
      return this.i18n(
          'importPasswordsDescriptionAccount',
          this.i18n('localPasswordManager'), this.accountEmail);
    }
    return this.i18n('importPasswordsDescriptionDevice');
  }

  private onBannerClick_() {
    if (this.isAccountStoreUser && this.isState_(DialogState.NO_DIALOG)) {
      recordPasswordsImportInteraction(
          PasswordsImportDesktopInteractions.UPM_STORE_PICKER_OPENED);
      this.dialogState_ = DialogState.STORE_PICKER;
    }
  }

  private closeDialog_() {
    this.dialogState_ = DialogState.NO_DIALOG;
    // When the dialog closes, restore focus to the element that opened it.
    // For account-store users, this is the entire link-row. For other users,
    // it's the "Select file" button. A timeout is needed to ensure that focus
    // is restored only after the dialog has completely closed.
    setTimeout(() => {
      if (this.shouldHideSelectFileButton_()) {
        this.$.linkRow.focus();
      } else {
        this.$.selectFileButtonLinkRow.focus();
      }
    });
  }

  private async resetImporter() {
    let deleteFile = false;
    if (this.isState_(DialogState.SUCCESS) &&
        !this.shouldHideDeleteFileOption_()) {
      // Trigger the file deletion if checkbox is ticked in SUCCESS (with no
      // errors) state.
      const deleteFileOption =
          this.shadowRoot!.querySelector<CrCheckboxElement>(
              '#deleteFileOption');
      assert(deleteFileOption);
      deleteFile = deleteFileOption.checked;
      chrome.metricsPrivate.recordBoolean(
          'PasswordManager.Import.FileDeletionSelected', deleteFile);
    }
    await this.passwordManager_.resetImporter(deleteFile);
  }

  private async onCloseClick_() {
    if (this.isState_(DialogState.CONFLICTS)) {
      recordPasswordsImportInteraction(
          PasswordsImportDesktopInteractions.CONFLICTS_CANCELED);
    }
    await this.resetImporter();
    this.closeDialog_();
  }

  private async onViewPasswordsClick_() {
    recordPasswordsImportInteraction(
        PasswordsImportDesktopInteractions.UPM_VIEW_PASSWORDS_CLICKED);
    await this.resetImporter();
    this.closeDialog_();
    Router.getInstance().navigateTo(Page.PASSWORDS);
  }

  private async selectFileHelper_() {
    // Clear selected rows from previous import, so it won’t affect the
    // following import.
    this.conflictsSelectedForReplace_ = [];
    this.dialogState_ = DialogState.IN_PROGRESS;
    // For "non-account-store-users" users passwords are stored in the "profile"
    // (DEVICE) store.
    let destinationStore = chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
    if (this.isAccountStoreUser) {
      const storePicker =
          this.shadowRoot!.querySelector<HTMLSelectElement>('#storePicker');
      assert(storePicker);
      this.selectedStoreOption_ = storePicker.value;
      this.passwordsSavedToAccount_ = this.selectedStoreOption_ ===
          chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
      if (this.passwordsSavedToAccount_) {
        destinationStore = chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
      }
    }

    this.results_ =
        await this.passwordManager_.importPasswords(destinationStore);
    await this.processResults_();
  }

  private async onSelectFileClick_() {
    recordPasswordsImportInteraction(
        PasswordsImportDesktopInteractions.UPM_FILE_SELECT_LAUNCHED);
    await this.selectFileHelper_();
  }

  private async continueImportHelper_(selectedIds: number[]) {
    this.dialogState_ = DialogState.IN_PROGRESS;
    // Close the dialog while import is in progress.
    this.results_ = await this.passwordManager_.continueImport(selectedIds);
    if (this.results_.status ===
        chrome.passwordsPrivate.ImportResultsStatus.DISMISSED) {
      recordPasswordsImportInteraction(
          PasswordsImportDesktopInteractions.CONFLICTS_REAUTH_FAILED);
      // When re-auth fails, restore the conflicts dialog.
      this.dialogState_ = DialogState.CONFLICTS;
      return;
    }
    await this.processResults_();
  }

  private async onSkipClick_() {
    recordPasswordsImportInteraction(
        PasswordsImportDesktopInteractions.CONFLICTS_SKIP_CLICKED);
    await this.continueImportHelper_(/*selectedIds=*/[]);
  }

  private async onReplaceClick_() {
    recordPasswordsImportInteraction(
        PasswordsImportDesktopInteractions.CONFLICTS_REPLACE_CLICKED);
    await this.continueImportHelper_(this.conflictsSelectedForReplace_);
  }

  private isPreviewItemChecked_(id: number): boolean {
    return this.conflictsSelectedForReplace_.includes(id);
  }

  /**
   * Handler for ticking conflicting password checkbox.
   */
  private onPasswordSelectedChange_(): void {
    this.conflictsSelectedForReplace_ =
        Array.from(this.shadowRoot!.querySelectorAll('password-preview-item'))
            .filter(item => item.checked)
            .map(item => item.passwordId);
  }

  private shouldDisableReplace_(): boolean {
    return !this.conflictsSelectedForReplace_.length;
  }

  private async processResults_() {
    assert(this.results_);
    switch (this.results_.status) {
      case chrome.passwordsPrivate.ImportResultsStatus.SUCCESS:
        await this.handleSuccess_();
        return;
      case chrome.passwordsPrivate.ImportResultsStatus.MAX_FILE_SIZE:
      case chrome.passwordsPrivate.ImportResultsStatus.IO_ERROR:
      case chrome.passwordsPrivate.ImportResultsStatus.UNKNOWN_ERROR:
      case chrome.passwordsPrivate.ImportResultsStatus.NUM_PASSWORDS_EXCEEDED:
      case chrome.passwordsPrivate.ImportResultsStatus.BAD_FORMAT:
        this.dialogState_ = DialogState.ERROR;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.CONFLICTS:
        this.conflictsDialogTitle_ =
            await PluralStringProxyImpl.getInstance().getPluralString(
                'importPasswordsConflictsTitle',
                this.results_.displayedEntries.length);
        this.conflicts_ = this.results_.displayedEntries;
        this.dialogState_ = DialogState.CONFLICTS;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.IMPORT_ALREADY_ACTIVE:
        this.dialogState_ = DialogState.ALREADY_ACTIVE;
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.DISMISSED:
        this.dialogState_ = DialogState.NO_DIALOG;
        break;
      default:
        assertNotReached();
    }
  }

  private getFailedImportsWithKnownErrors_():
      chrome.passwordsPrivate.ImportEntry[] {
    assert(this.results_);
    return this.results_.displayedEntries.filter(
        (entry) => entry.status !==
            chrome.passwordsPrivate.ImportEntryStatus.UNKNOWN_ERROR);
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
      let descriptionText =
          await PluralStringProxyImpl.getInstance().getPluralString(
              'importPasswordsSuccessSummaryAccount',
              this.results_.numberImported);
      descriptionText =
          descriptionText.replace('$1', this.i18n('localPasswordManager'));
      this.successDescription_ =
          descriptionText.replace('$2', this.accountEmail);
    } else {
      const descriptionText =
          await PluralStringProxyImpl.getInstance().getPluralString(
              'importPasswordsSuccessSummaryDevice',
              this.results_.numberImported);
      this.successDescription_ =
          descriptionText.replace('$1', this.i18n('localPasswordManager'));
    }

    this.dialogState_ = DialogState.SUCCESS;
  }

  private getSuccessDialogTitle_(): string {
    assert(this.results_);
    return this.results_.displayedEntries.length ?
        this.i18n('importPasswordsCompleteTitle') :
        this.i18n('importPasswordsSuccessTitle');
  }

  private getErrorDialogDescription_(): TrustedHTML {
    assert(this.results_);
    switch (this.results_.status) {
      case chrome.passwordsPrivate.ImportResultsStatus.MAX_FILE_SIZE:
        return this.i18nAdvanced('importPasswordsFileSizeExceeded');
      case chrome.passwordsPrivate.ImportResultsStatus.IO_ERROR:
      case chrome.passwordsPrivate.ImportResultsStatus.UNKNOWN_ERROR:
        return this.i18nAdvanced('importPasswordsUnknownError');
      case chrome.passwordsPrivate.ImportResultsStatus.NUM_PASSWORDS_EXCEEDED:
        return this.i18nAdvanced('importPasswordsLimitExceeded');
      case chrome.passwordsPrivate.ImportResultsStatus.BAD_FORMAT:
        return this.i18nAdvanced('importPasswordsBadFormatError', {
          attrs: ['class'],
          substitutions: [
            this.results_.fileName,
            loadTimeData.getString('importPasswordsHelpURL'),
          ],
        });
      default:
        assertNotReached();
    }
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

  private shouldHideLinkRowIcon_(): boolean {
    return !this.isAccountStoreUser || this.isState_(DialogState.IN_PROGRESS);
  }

  private shouldHideSelectFileButton_(): boolean {
    return this.isAccountStoreUser || this.isState_(DialogState.IN_PROGRESS);
  }

  private shouldHideDeleteFileOption_(): boolean {
    // "Delete file" checkbox is only shown in "success" state if all passwords
    // were imported.
    assert(this.results_);
    return !!this.results_.displayedEntries.length;
  }

  private shouldHideFailuresSummary_(): boolean {
    assert(this.results_);
    return !this.results_.displayedEntries.length;
  }

  private getStoreOptionAccountText_(): string {
    assert(this.accountEmail);
    return this.i18n(
        'passwordsStoreOptionAccount', this.i18n('localPasswordManager'),
        this.accountEmail);
  }

  private getFailedEntryErrorMessage_(
      status: chrome.passwordsPrivate.ImportEntryStatus): string {
    // TODO(crbug.com/40264637): Use constants for length limits.
    switch (status) {
      case chrome.passwordsPrivate.ImportEntryStatus.MISSING_PASSWORD:
        return this.i18n('importPasswordsMissingPassword');
      case chrome.passwordsPrivate.ImportEntryStatus.MISSING_URL:
        return this.i18n('importPasswordsMissingURL');
      case chrome.passwordsPrivate.ImportEntryStatus.INVALID_URL:
        return this.i18n('importPasswordsInvalidURL');
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_URL:
        return this.i18n('importPasswordsLongURL');
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_PASSWORD:
        return this.i18n('importPasswordsLongPassword');
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_USERNAME:
        return this.i18n('importPasswordsLongUsername');
      case chrome.passwordsPrivate.ImportEntryStatus.CONFLICT_PROFILE:
        if (this.isUserSyncingPasswords) {
          return this.i18n(
              'importPasswordsConflictAccount',
              this.i18n('localPasswordManager'), this.accountEmail);
        }
        return this.i18n('importPasswordsConflictDevice');
      case chrome.passwordsPrivate.ImportEntryStatus.CONFLICT_ACCOUNT:
        return this.i18n(
            'importPasswordsConflictAccount', this.i18n('localPasswordManager'),
            this.accountEmail);
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_NOTE:
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_CONCATENATED_NOTE:
        return this.i18n('importPasswordsLongNote');
      case chrome.passwordsPrivate.ImportEntryStatus.UNKNOWN_ERROR:
      case chrome.passwordsPrivate.ImportEntryStatus.NON_ASCII_URL:
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-importer': PasswordsImporterElement;
  }
}

customElements.define(PasswordsImporterElement.is, PasswordsImporterElement);
