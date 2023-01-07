// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-passkeys-delete-confirmation-dialog' is a
 * component for confirming that the user wants to delete a passkey.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './passkeys_delete_confirmation_dialog.html.js';

export interface SettingsPasskeysDeleteConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
    cancelButton: CrButtonElement,
    deleteButton: CrButtonElement,
  };
}

export class SettingsPasskeysDeleteConfirmationDialogElement extends
    PolymerElement {
  static get is() {
    return 'settings-passkeys-delete-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  /**
   * @return true when the user selected 'Delete'.
   */
  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  private cancel_() {
    this.$.dialog.cancel();
  }

  private delete_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-passkeys-delete-confirmation-dialog':
        SettingsPasskeysDeleteConfirmationDialogElement;
  }
}

customElements.define(
    SettingsPasskeysDeleteConfirmationDialogElement.is,
    SettingsPasskeysDeleteConfirmationDialogElement);
