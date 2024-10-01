// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './passpoint_remove_dialog.html.js';

const PasspointRemoveDialogElementBase = I18nMixin(PolymerElement);

export class PasspointRemoveDialogElement extends
    PasspointRemoveDialogElementBase {
  static get is() {
    return 'passpoint-remove-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  constructor() {
    super();
  }

  open(): void {
    const dialog = this.getDialog_();
    if (!dialog.open) {
      dialog.showModal();
    }

    this.shadowRoot!.querySelector<CrButtonElement>('#confirmButton')!.focus();
  }

  close(): void {
    const dialog = this.getDialog_();
    if (dialog.open) {
      dialog.close();
    }
  }

  private getDialog_(): CrDialogElement {
    return castExists(
        this.shadowRoot!.querySelector<CrDialogElement>('#dialog'));
  }

  private onCancelClick_(): void {
    this.getDialog_().cancel();
  }

  private onConfirmClick_(): void {
    const event = new CustomEvent('confirm', {bubbles: true, composed: true});
    this.dispatchEvent(event);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PasspointRemoveDialogElement.is]: PasspointRemoveDialogElement;
  }
}

customElements.define(
    PasspointRemoveDialogElement.is, PasspointRemoveDialogElement);
