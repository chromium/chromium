// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './password_check_edit_disclaimer_dialog.html.js';

export interface SettingsPasswordEditDisclaimerDialogElement {
  $: {
    dialog: CrDialogElement,
    edit: CrButtonElement,
  };
}

const SettingsPasswordEditDisclaimerDialogElementBase =
    I18nMixin(PolymerElement);

export class SettingsPasswordEditDisclaimerDialogElement extends
    SettingsPasswordEditDisclaimerDialogElementBase {
  static get is() {
    return 'settings-password-edit-disclaimer-dialog';
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
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-password-edit-disclaimer-dialog':
        SettingsPasswordEditDisclaimerDialogElement;
  }
}

customElements.define(
    SettingsPasswordEditDisclaimerDialogElement.is,
    SettingsPasswordEditDisclaimerDialogElement);
