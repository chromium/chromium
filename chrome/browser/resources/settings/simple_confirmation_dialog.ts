// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-simple-confirmation-dialog' is a generic
 * component for confirming an action the user has taken, given them an
 * opportunity to cancel.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './simple_confirmation_dialog.html.js';

export interface SettingsSimpleConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
    cancel: HTMLElement,
    confirm: HTMLElement,
  };
}

export class SettingsSimpleConfirmationDialogElement extends PolymerElement {
  static get is() {
    return 'settings-simple-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      titleText: String,
      bodyText: String,
      confirmText: String,

      noPrimaryButton: {
        type: Boolean,
        value: false,
      },
    };
  }

  titleText: string;
  bodyText: string;
  confirmText: string;
  noPrimaryButton: boolean;

  /** @return Whether the user confirmed the dialog. */
  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onConfirmClick_() {
    this.$.dialog.close();
  }

  private getConfirmButtonCssClass_(): string {
    return this.noPrimaryButton ? '' : 'action-button';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-simple-confirmation-dialog':
        SettingsSimpleConfirmationDialogElement;
  }
}

customElements.define(
    SettingsSimpleConfirmationDialogElement.is,
    SettingsSimpleConfirmationDialogElement);
