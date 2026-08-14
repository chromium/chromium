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

import type {ImportEntry, ImportResults, PasswordManagerProxy} from './password_manager_proxy.js';
import {ImportEntryStatus, ImportResultsStatus, PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './passwords_importer.html.js';
import {Page, Router} from './router.js';
import {UserUtilMixin} from './user_utils_mixin.js';

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

const PasswordsImporterElementBase = UserUtilMixin(I18nMixin(PolymerElement));

export class PasswordsImporterElement extends PasswordsImporterElementBase {
  static get is() {
    return 'passwords-importer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
        computed: 'computeBannerDescription_(isSyncingPasswords,' +
            'isAccountStoreUser, accountEmail)',
      },
    };
  }

  static get observers() {
    return [
      'updateDefaultStore_(isAccountStoreUser)',
      'updatePasswordsSavedToAccount_(isSyncingPasswords)',
    ];
  }

  declare private dialogState_: DialogState;
  // Refers both to syncing users with sync enabled for passwords and account
  // store users who choose to import passwords to their account.
  private passwordsSavedToAccount_: boolean;
  declare private selectedStoreOption_: string;
  declare private bannerDescription_: string;
  declare private results_: ImportResults|null;
  declare private conflicts_: ImportEntry[];
  declare private conflictsSelectedForReplace_: number[];
  declare private successDescription_: string;
  declare private conflictsDialogTitle_: string;
  declare private failedImportsSummary_: string;
  declare private rowsWithUnknownErrorsSummary_: string;
  declare private showRowsWithUnknownErrorsSummary_: boolean;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  launchImport() {
    this.executeIfTrustedVaultUnlocked(() => {
      this.dialogState_ = DialogState.IN_PROGRESS;
      // Timeout is needed to allow Polymer to render the Settings page before
      // the system file picker has been opened.
      setTimeout(() => {
        if (this.isAccountStoreUser) {
          this.dialogState_ = DialogState.STORE_PICKER;
        } else {
          this.selectFileHelper_();
        }
      }, 200);
    });
  }

  private updateDefaultStore_() {
    if (this.isAccountStoreUser) {
      this.selectedStoreOption_ =
          chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
    }
  }

  private updatePasswordsSavedToAccount_() {
    this.passwordsSavedToAccount_ = this.isSyncingPasswords;
  }

  private isState_(state: DialogState): boolean {
    return this.dialogState_ === state;
  }

  private computeBannerDescription_(): string {
    if (this.isAccountStoreUser) {
      return this.i18n('importPasswordsGenericDescription');
    }
    if (this.isSyncingPasswords) {
      return this.i18n(
          'importPasswordsDescriptionAccount',
          this.i18n('localPasswordManager'), this.accountEmail);
    }
    return this.i18n('importPasswordsDescriptionDevice');
  }

  private getAriaDescription_(bannerDescription: string): string {
    return [
      this.i18n('importPasswords'),
      bannerDescription,
    ].join('. ');
  }

  private onBannerClick_() {
    this.executeIfTrustedVaultUnlocked(() => {
      if (this.isAccountStoreUser && this.isState_(DialogState.NO_DIALOG)) {
        this.dialogState_ = DialogState.STORE_PICKER;
      }
    });
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
    }
    await this.passwordManager_.resetImporter(deleteFile);
  }

  private async onCloseClick_() {
    await this.resetImporter();
    this.closeDialog_();
  }

  private async onViewPasswordsClick_() {
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

  private onSelectFileClick_() {
    this.executeIfTrustedVaultUnlocked(() => {
      this.selectFileHelper_();
    });
  }

  private async continueImportHelper_(selectedIds: number[]) {
    this.dialogState_ = DialogState.IN_PROGRESS;
    // Close the dialog while import is in progress.
    this.results_ = await this.passwordManager_.continueImport(selectedIds);
    if (this.results_.status === ImportResultsStatus.kDismissed) {
      // When re-auth fails, restore the conflicts dialog.
      this.dialogState_ = DialogState.CONFLICTS;
      return;
    }
    await this.processResults_();
  }

  private async onSkipClick_() {
    await this.continueImportHelper_(/*selectedIds=*/[]);
  }

  private async onReplaceClick_() {
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
      case ImportResultsStatus.kSuccess:
        await this.handleSuccess_();
        return;
      case ImportResultsStatus.kMaxFileSize:
      case ImportResultsStatus.kIoError:
      case ImportResultsStatus.kUnknownError:
      case ImportResultsStatus.kNumPasswordsExceeded:
      case ImportResultsStatus.kBadFormat:
        this.dialogState_ = DialogState.ERROR;
        break;
      case ImportResultsStatus.kConflicts:
        this.conflictsDialogTitle_ =
            await PluralStringProxyImpl.getInstance().getPluralString(
                'importPasswordsConflictsTitle',
                this.results_.displayedEntries.length);
        this.conflicts_ = this.results_.displayedEntries;
        this.dialogState_ = DialogState.CONFLICTS;
        break;
      case ImportResultsStatus.kImportAlreadyActive:
        this.dialogState_ = DialogState.ALREADY_ACTIVE;
        break;
      case ImportResultsStatus.kDismissed:
        this.dialogState_ = DialogState.NO_DIALOG;
        break;
      default:
        assertNotReached();
    }
  }

  private getFailedImportsWithKnownErrors_(): ImportEntry[] {
    assert(this.results_);
    return this.results_.displayedEntries.filter(
        (entry) => entry.status !== ImportEntryStatus.kUnknownError);
  }

  private async handleSuccess_() {
    assert(this.results_);
    if (this.results_.displayedEntries.length) {
      const rowsWithUnknownErrorCount =
          this.results_.displayedEntries
              .filter(
                  (entry) => entry.status === ImportEntryStatus.kUnknownError)
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
      case ImportResultsStatus.kMaxFileSize:
        return this.i18nAdvanced('importPasswordsFileSizeExceeded');
      case ImportResultsStatus.kIoError:
      case ImportResultsStatus.kUnknownError:
        return this.i18nAdvanced('importPasswordsUnknownError');
      case ImportResultsStatus.kNumPasswordsExceeded:
        return this.i18nAdvanced('importPasswordsLimitExceeded');
      case ImportResultsStatus.kBadFormat:
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

  private getFailedEntryErrorMessage_(status: ImportEntryStatus): string {
    // TODO(crbug.com/40264637): Use constants for length limits.
    switch (status) {
      case ImportEntryStatus.kMissingPassword:
        return this.i18n('importPasswordsMissingPassword');
      case ImportEntryStatus.kMissingUrl:
        return this.i18n('importPasswordsMissingURL');
      case ImportEntryStatus.kInvalidUrl:
        return this.i18n('importPasswordsInvalidURL');
      case ImportEntryStatus.kLongUrl:
        return this.i18n('importPasswordsLongURL');
      case ImportEntryStatus.kLongPassword:
        return this.i18n('importPasswordsLongPassword');
      case ImportEntryStatus.kLongUsername:
        return this.i18n('importPasswordsLongUsername');
      case ImportEntryStatus.kConflictProfile:
        if (this.isSyncingPasswords) {
          return this.i18n(
              'importPasswordsConflictAccount',
              this.i18n('localPasswordManager'), this.accountEmail);
        }
        return this.i18n('importPasswordsConflictDevice');
      case ImportEntryStatus.kConflictAccount:
        return this.i18n(
            'importPasswordsConflictAccount', this.i18n('localPasswordManager'),
            this.accountEmail);
      case ImportEntryStatus.kLongNote:
      case ImportEntryStatus.kLongConcatenatedNote:
        return this.i18n('importPasswordsLongNote');
      case ImportEntryStatus.kUnknownError:
      case ImportEntryStatus.kNonAsciiUrl:
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
