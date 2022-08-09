// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-import-dialog' is the dialog that allows importing
 * passwords.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
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
  };
}

const PasswordsImportDialogElementBase = I18nMixin(PolymerElement);

export enum ImportDialogState {
  START,
  ERROR,
  SUCCESS,
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

      descriptionText_: String,

      results_: Object,
    };
  }

  dialogState: ImportDialogState;
  private results_: chrome.passwordsPrivate.ImportResults|null;
  private descriptionText_: string;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.descriptionText_ = this.i18n('importPasswordsGenericDescription');
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

  /**
   * Handler for clicking the 'chooseFile' button. It triggers import flow.
   */
  private async onChooseFileClick_() {
    this.results_ = await this.passwordManager_.importPasswords(
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE);
    switch (this.results_.status) {
      case chrome.passwordsPrivate.ImportResultsStatus.SUCCESS:
        this.handleSuccess_();
        break;
      case chrome.passwordsPrivate.ImportResultsStatus.IO_ERROR:
        this.descriptionText_ = this.i18n('importPasswordsUnknownError');
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
      default:
        assertNotReached();
    }
  }

  private async handleSuccess_() {
    assert(this.results_);
    this.descriptionText_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'importPasswordsSuccessSummaryDevice',
            this.results_.numberImported);
    this.dialogState = ImportDialogState.SUCCESS;
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
    switch (status) {
      case chrome.passwordsPrivate.ImportEntryStatus.MISSING_PASSWORD:
        return this.i18n('importPasswordsMissingPassword');
      case chrome.passwordsPrivate.ImportEntryStatus.MISSING_URL:
        return this.i18n('importPasswordsMissingURL');
      case chrome.passwordsPrivate.ImportEntryStatus.INVALID_URL:
        return this.i18n('importPasswordsInvalidURL');
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_PASSWORD:
        return this.i18n('importPasswordsLongPassword');
      case chrome.passwordsPrivate.ImportEntryStatus.LONG_USERNAME:
        return this.i18n('importPasswordsLongUsername');
      case chrome.passwordsPrivate.ImportEntryStatus.CONFLICT_PROFILE:
        // TODO(crbug/1325290): for syncing users this should be "account
        // conflict".
        return this.i18n('importPasswordsConflictDevice');
      case chrome.passwordsPrivate.ImportEntryStatus.CONFLICT_ACCOUNT:
        // TODO(crbug/1325290): fill with real data.
        return this.i18n('importPasswordsConflictAccount', '');
    }
    assertNotReached();
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
