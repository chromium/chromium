// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'address-remove-confirmation-dialog' is the dialog that allows
 * removing a saved address.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './address_remove_confirmation_dialog.html.js';


export interface SettingsAddressRemoveConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
    remove: HTMLElement,
    cancel: HTMLElement,
  };
}

export class SettingsAddressRemoveConfirmationDialogElement extends
    PolymerElement {
  static get is() {
    return 'settings-address-remove-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  private onRemoveClick() {
    this.$.dialog.close();
  }

  private onCancelClick() {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-address-remove-confirmation-dialog':
        SettingsAddressRemoveConfirmationDialogElement;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-address-remove-confirmation-dialog':
        SettingsAddressRemoveConfirmationDialogElement;
  }
}

customElements.define(
    SettingsAddressRemoveConfirmationDialogElement.is,
    SettingsAddressRemoveConfirmationDialogElement);
