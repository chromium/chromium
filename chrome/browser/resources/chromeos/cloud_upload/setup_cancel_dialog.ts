// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {getTemplate} from './setup_cancel_dialog.html.js';

/**
 * The SetupCancelDialogElement represents the confirmation dialog shown when
 * the user wants to cancel the setup part way through.
 */
export class SetupCancelDialogElement extends HTMLElement {
  private dialog: CrDialogElement;

  /** Callback called if the user chooses to cancel the setup. */
  private cancelCallback: (() => void)|null = null;

  constructor() {
    super();

    this.attachShadow({mode: 'open'}).innerHTML = getTemplate();
    this.dialog = this.$('cr-dialog');
    this.$('.action-button')!.addEventListener(
        'click', () => this.onResumeButtonClick());
    this.$('.cancel-button')!.addEventListener(
        'click', () => this.onCancelButtonClick());
  }

  $<T extends HTMLElement>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  get open(): boolean {
    return this.dialog.open;
  }

  show(cancelCallback: () => void): void {
    this.cancelCallback = cancelCallback;
    this.dialog.showModal();
  }

  private onResumeButtonClick(): void {
    this.dialog.close();
  }

  private onCancelButtonClick(): void {
    this.cancelCallback!();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'setup-cancel-dialog': SetupCancelDialogElement;
  }
}

customElements.define('setup-cancel-dialog', SetupCancelDialogElement);
