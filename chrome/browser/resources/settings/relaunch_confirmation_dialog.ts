// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {LifetimeBrowserProxyImpl} from '/shared/settings/lifetime_browser_proxy.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './relaunch_confirmation_dialog.html.js';

export interface RelaunchConfirmationDialogElement {
  $: {
    cancel: CrButtonElement,
    confirm: CrButtonElement,
    dialog: CrDialogElement,
  };
}

/**
 * The element corresponding to <relaunch-confirmation-dialog>.
 * The dialog is only supported for "non" ChromeOS platforms and is
 * shown to warn users if they have any open Incognito windows before
 * proceeding with the restart/relaunch action.
 *
 * To make use of this dialog, add the below html to the target html.
 *
 * ${this.shouldShowRelaunchDialog ? html`
 *   <relaunch-confirmation-dialog @close="${this.onRelaunchDialogClose}">
 *   </relaunch-confirmation-dialog>
 * ` : ''}
 *
 * Then, in the corresponding typescript file, make the target HTMLElement
 * inherit from RelaunchMixinLit and invoke the member method
 * RelaunchMixinLit#performRestart where required. The restart happens in
 * RelaunchMixinLit#onRelaunchDialogClose if the dialog was confirmed.
 */
export class RelaunchConfirmationDialogElement extends CrLitElement {
  static get is() {
    return 'relaunch-confirmation-dialog';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      relaunchConfirmationDialogDesc: {type: String},

      //  Boolean that defines if the confirmation dialog is opened for browser
      //  version update.
      isVersionUpdate: {type: Boolean},
    };
  }

  accessor relaunchConfirmationDialogDesc: string|null = null;
  accessor isVersionUpdate: boolean = false;

  override async connectedCallback() {
    super.connectedCallback();
    this.relaunchConfirmationDialogDesc =
        await LifetimeBrowserProxyImpl.getInstance()
            .getRelaunchConfirmationDialogDescription(this.isVersionUpdate);
  }

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  protected onConfirmClick_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'relaunch-confirmation-dialog': RelaunchConfirmationDialogElement;
  }
}

customElements.define(
    RelaunchConfirmationDialogElement.is, RelaunchConfirmationDialogElement);
