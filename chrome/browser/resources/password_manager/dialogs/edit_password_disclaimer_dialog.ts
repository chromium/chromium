// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './edit_password_disclaimer_dialog.html.js';

export interface EditPasswordDisclaimerDialogElement {
  $: {
    dialog: CrDialogElement,
    edit: CrButtonElement,
  };
}

const EditPasswordDisclaimerDialogElementBase = I18nMixinLit(CrLitElement);

export class EditPasswordDisclaimerDialogElement extends
    EditPasswordDisclaimerDialogElementBase {
  static get is() {
    return 'edit-password-disclaimer-dialog';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The website origin that is being displayed.
       */
      origin: {type: String},
    };
  }

  accessor origin: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  protected onEditClick_() {
    this.fire('edit-password-click');
    this.$.dialog.close();
  }

  protected onCancelClick_() {
    this.$.dialog.close();
  }

  protected getDisclaimerTitle_(): string {
    return this.i18n('editDisclaimerTitle', this.origin);
  }

  protected getDisclaimerDescription_(): string {
    const brandingName = this.i18n('localPasswordManager');
    return this.i18n('editDisclaimerDescription', brandingName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'edit-password-disclaimer-dialog': EditPasswordDisclaimerDialogElement;
  }
}

customElements.define(
    EditPasswordDisclaimerDialogElement.is,
    EditPasswordDisclaimerDialogElement);
