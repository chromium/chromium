// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'local-iban-remove-confirmation-dialog' is the dialog
 * that allows removing a locally-saved IBAN.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './local_iban_remove_confirmation_dialog.html.js';


export interface SettingsLocalIbanRemoveConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
    remove: HTMLElement,
    cancel: HTMLElement,
  };
}

export class SettingsLocalIbanRemoveConfirmationDialogElement extends
    PolymerElement {
  static get is() {
    return 'settings-local-iban-remove-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  private onRemoveClick_() {
    this.$.dialog.close();
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-local-iban-remove-confirmation-dialog':
        SettingsLocalIbanRemoveConfirmationDialogElement;
  }
}

customElements.define(
    SettingsLocalIbanRemoveConfirmationDialogElement.is,
    SettingsLocalIbanRemoveConfirmationDialogElement);
