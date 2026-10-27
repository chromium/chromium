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
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './simple_confirmation_dialog.html.js';

export interface SettingsSimpleConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
    cancel: HTMLElement,
    confirm: HTMLElement,
  };
}

export class SettingsSimpleConfirmationDialogElement extends CrLitElement {
  static get is() {
    return 'settings-simple-confirmation-dialog';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      titleText: {type: String},
      bodyText: {type: String},
      confirmText: {type: String},
      noPrimaryButton: {type: Boolean},
    };
  }

  accessor titleText: string = '';
  accessor bodyText: string = '';
  accessor confirmText: string = '';
  accessor noPrimaryButton: boolean = false;

  /** @return Whether the user confirmed the dialog. */
  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  protected onConfirmClick_() {
    this.$.dialog.close();
  }

  protected getConfirmButtonCssClass_(): string {
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
