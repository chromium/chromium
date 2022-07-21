// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-import-dialog' is the dialog that allows importing
 * passwords.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {getTemplate} from './passwords_import_dialog.html.js';

export interface PasswordsImportDialogElement {
  $: {
    dialog: CrDialogElement,
    cancel: HTMLElement,
    chooseFile: HTMLElement,
  };
}

export class PasswordsImportDialogElement extends PolymerElement {
  static get is() {
    return 'passwords-import-dialog';
  }

  static get template() {
    return getTemplate();
  }

  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  }

  /**
   * Handler for clicking the 'chooseFile' button.
   */
  private onChooseFileClick_() {
    this.passwordManager_.importPasswords(
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE);
    this.close();
  }

  /**
   * Handler for clicking the 'cancel' button.
   */
  private onCancelClick_() {
    this.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-import-dialog': PasswordsImportDialogElement;
  }
}

customElements.define(
    PasswordsImportDialogElement.is, PasswordsImportDialogElement);