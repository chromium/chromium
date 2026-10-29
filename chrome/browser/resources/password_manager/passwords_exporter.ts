// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ExportPasswordsResult, ExportProgressStatus} from './password_manager.mojom-webui.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getCss} from './passwords_exporter.css.js';
import {getHtml} from './passwords_exporter.html.js';

export interface PasswordsExporterElement {
  $: {
    exportSuccessToast: CrToastElement,
  };
}

const PasswordsExporterElementBase = I18nMixinLit(CrLitElement);

export class PasswordsExporterElement extends PasswordsExporterElementBase {
  static get is() {
    return 'passwords-exporter';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** Whether password export progress spinner is shown. */
      showExportInProgress_: {type: Boolean},

      /** Whether password export error dialog is shown. */
      showPasswordsExportErrorDialog_: {type: Boolean},

      /** The error that occurred while exporting. */
      exportErrorMessage_: {type: String},
    };
  }

  protected accessor showPasswordsExportErrorDialog_: boolean = false;
  protected accessor showExportInProgress_: boolean = false;
  protected accessor exportErrorMessage_: string|null = null;

  private exportListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();

    const proxy = PasswordManagerImpl.getInstance();
    // If export started on a different tab and is still in progress, display a
    // busy UI.
    proxy.requestExportProgressStatus().then(status => {
      if (status === ExportProgressStatus.kInProgress) {
        this.showExportInProgress_ = true;
      }
    });

    this.exportListenerId_ =
        proxy.callbackRouter.onPasswordsExportProgress.addListener(
            this.onPasswordsFileExportProgress_.bind(this));
  }

  override disconnectedCallback() {
    const proxy = PasswordManagerImpl.getInstance();
    assert(this.exportListenerId_ !== null);
    proxy.callbackRouter.removeListener(this.exportListenerId_);
    this.exportListenerId_ = null;
    super.disconnectedCallback();
  }

  /**
   * Tells the PasswordsPrivate API to export saved passwords in a .csv file.
   */
  protected onExportClick_() {
    PasswordManagerImpl.getInstance().exportPasswords().then(
        (result: ExportPasswordsResult) => {
          if (result === ExportPasswordsResult.kInProgress) {
            // Exporting was started by a different call to exportPasswords()
            // and is still in progress. This UI needs to be updated to the
            // current status.
            this.showExportInProgress_ = true;
          }
        });
  }

  /**
   * Closes the export error dialog.
   */
  protected onCancelButtonClick_() {
    this.showPasswordsExportErrorDialog_ = false;
  }

  /**
   * Retries export from the error dialog.
   */
  protected onTryAgainClick_() {
    this.onCancelButtonClick_();
    this.onExportClick_();
  }

  /**
   * Handles an export progress event by showing the progress spinner or caching
   * the event for later consumption.
   */
  private onPasswordsFileExportProgress_(
      status: ExportProgressStatus, folderName: string|null) {
    if (status === ExportProgressStatus.kInProgress) {
      this.showExportInProgress_ = true;
      return;
    }

    this.showExportInProgress_ = false;

    switch (status) {
      case ExportProgressStatus.kSucceeded:
        this.$.exportSuccessToast.show();
        break;
      case ExportProgressStatus.kFailedWrite:
        assert(folderName);
        this.exportErrorMessage_ =
            this.i18n('exportPasswordsFailTitle', folderName);
        this.showPasswordsExportErrorDialog_ = true;
        break;
      default:
        break;
    }
  }

  protected onOpenInShellButtonClick_() {
    PasswordManagerImpl.getInstance().showLastExportedFileInShell();
    this.$.exportSuccessToast.hide();
  }

  protected getAriaDescription_(): string {
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
