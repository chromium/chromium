// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './shared_style.css.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {PasswordsFileExportProgressListener} from './password_manager_proxy.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './passwords_exporter.html.js';

const ProgressStatus = chrome.passwordsPrivate.ExportProgressStatus;

export interface PasswordsExporterElement {
  $: {
    exportSuccessToast: CrToastElement,
  };
}

const PasswordsExporterElementBase = I18nMixin(PolymerElement);

export class PasswordsExporterElement extends PasswordsExporterElementBase {
  static get is() {
    return 'passwords-exporter';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Whether password export progress spinner is shown. */
      showExportInProgress_: {
        type: Boolean,
        value: false,
      },

      /** Whether password export error dialog is shown. */
      showExportErrorDialog_: {
        type: Boolean,
        value: false,
      },

      /** The error that occurred while exporting. */
      exportErrorMessage_: {
        type: String,
        value: null,
      },
    };
  }

  private onPasswordsFileExportProgressListener_:
      PasswordsFileExportProgressListener|null = null;

  private showPasswordsExportErrorDialog_: boolean;
  private showExportInProgress_: boolean;
  private exportErrorMessage_: string|null;
  private exportedFilePath_: string|null;


  override connectedCallback() {
    super.connectedCallback();

    // If export started on a different tab and is still in progress, display a
    // busy UI.
    PasswordManagerImpl.getInstance().requestExportProgressStatus().then(
        status => {
          if (status === ProgressStatus.IN_PROGRESS) {
            this.showExportInProgress_ = true;
          }
        });

    this.onPasswordsFileExportProgressListener_ =
        (progress: chrome.passwordsPrivate.PasswordExportProgress) =>
            this.onPasswordsFileExportProgress_(progress);
    PasswordManagerImpl.getInstance().addPasswordsFileExportProgressListener(
        this.onPasswordsFileExportProgressListener_);
  }

  override disconnectedCallback() {
    assert(this.onPasswordsFileExportProgressListener_);
    PasswordManagerImpl.getInstance().removePasswordsFileExportProgressListener(
        this.onPasswordsFileExportProgressListener_);
    super.disconnectedCallback();
  }

  /**
   * Tells the PasswordsPrivate API to export saved passwords in a .csv file.
   */
  private onExportClick_() {
    PasswordManagerImpl.getInstance().exportPasswords().catch((error) => {
      if (error === 'in-progress') {
        // Exporting was started by a different call to exportPasswords() and is
        // is still in progress. This UI needs to be updated to the current
        // status.
        this.showExportInProgress_ = true;
      }
    });
  }

  /**
   * Closes the export error dialog.
   */
  private closePasswordsExportErrorDialog_() {
    this.showPasswordsExportErrorDialog_ = false;
  }

  /**
   * Retries export from the error dialog.
   */
  private onTryAgainClick_() {
    this.closePasswordsExportErrorDialog_();
    this.onExportClick_();
  }

  /**
   * Handles an export progress event by showing the progress spinner or caching
   * the event for later consumption.
   */
  private onPasswordsFileExportProgress_(
      progress: chrome.passwordsPrivate.PasswordExportProgress) {
    if (progress.status === ProgressStatus.IN_PROGRESS) {
      this.showExportInProgress_ = true;
      return;
    }

    this.showExportInProgress_ = false;

    switch (progress.status) {
      case ProgressStatus.SUCCEEDED:
        assert(progress.filePath);
        this.exportedFilePath_ = progress.filePath;
        this.$.exportSuccessToast.show();
        break;
      case ProgressStatus.FAILED_WRITE_FAILED:
        assert(progress.folderName);
        this.exportErrorMessage_ =
            this.i18n('exportPasswordsFailTitle', progress.folderName);
        this.showPasswordsExportErrorDialog_ = true;
        break;
    }
  }

  private onOpenInShellButtonClick_() {
    assert(this.exportedFilePath_);
    PasswordManagerImpl.getInstance().showExportedFileInShell(
        this.exportedFilePath_);
    this.$.exportSuccessToast.hide();
  }

  private getAriaLabel_(): string {
    return [
      this.i18n('exportPasswords'),
      this.i18n('exportPasswordsDescription'),
    ].join('. ');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-exporter': PasswordsExporterElement;
  }
}

customElements.define(PasswordsExporterElement.is, PasswordsExporterElement);
