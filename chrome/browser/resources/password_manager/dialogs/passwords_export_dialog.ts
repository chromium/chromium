// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-export-dialog' is the dialog that allows exporting
 * passwords.
 */

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from '../password_manager_proxy.js';

import {getTemplate} from './passwords_export_dialog.html.js';

/**
 * The states of the export passwords dialog.
 */
enum States {
  START = 'START',
  IN_PROGRESS = 'IN_PROGRESS',
  ERROR = 'ERROR',
}

const ProgressStatus = chrome.passwordsPrivate.ExportProgressStatus;

const PasswordsExportDialogElementBase = I18nMixin(PolymerElement);

export class PasswordsExportDialogElement extends
    PasswordsExportDialogElementBase {
  static get is() {
    return 'passwords-export-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showStartDialog_: Boolean,
    };
  }

  private showStartDialog_: boolean;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override ready() {
    super.ready();
    this.addEventListener('cancel', this.close_);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.switchToDialog_(States.START);

    // If export started on a different tab and is still in progress, display a
    // busy UI.
    this.passwordManager_.requestExportProgressStatus().then(status => {
      if (status === ProgressStatus.IN_PROGRESS) {
        // TODO(crbug.com/1394416): Show progress dialog once implemented.
        return;
      }
    });
  }

  /** Closes the dialog. */
  private close_() {
    this.showStartDialog_ = false;
    // Need to allow for the dialogs to be removed from the DOM before firing
    // the close event. Otherwise the handler will not be able to set focus.
    microTask.run(
        () => this.dispatchEvent(new CustomEvent(
            'passwords-export-dialog-close', {bubbles: true, composed: true})));
  }

  /**
   * Opens the specified dialog and hides the others.
   * @param state the dialog to open.
   */
  private switchToDialog_(state: States) {
    this.showStartDialog_ = state === States.START;
  }

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   */
  private onCancelButtonTap_() {
    this.close_();
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'passwords-export-dialog': PasswordsExportDialogElement;
  }
}

customElements.define(
    PasswordsExportDialogElement.is, PasswordsExportDialogElement);