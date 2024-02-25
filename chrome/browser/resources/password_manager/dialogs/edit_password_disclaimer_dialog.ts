// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './edit_password_disclaimer_dialog.html.js';

export interface EditPasswordDisclaimerDialogElement {
  $: {
    dialog: CrDialogElement,
    edit: CrButtonElement,
  };
}

const EditPasswordDisclaimerDialogElementBase = I18nMixin(PolymerElement);

export class EditPasswordDisclaimerDialogElement extends
    EditPasswordDisclaimerDialogElementBase {
  static get is() {
    return 'edit-password-disclaimer-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The website origin that is being displayed.
       */
      origin: String,
    };
  }

  origin: string;

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private onEditClick_() {
    this.dispatchEvent(new CustomEvent(
        'edit-password-click', {bubbles: true, composed: true}));
    this.$.dialog.close();
  }

  private onCancel_() {
    this.$.dialog.close();
  }

  private getDisclaimerTitle_(): string {
    return this.i18n('editDisclaimerTitle', this.origin);
  }

  private getDisclaimerDescription_(): string {
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
