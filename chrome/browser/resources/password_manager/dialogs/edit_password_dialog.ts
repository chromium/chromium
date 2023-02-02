// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../shared_style.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ShowPasswordMixin} from '../show_password_mixin.js';

import {getTemplate} from './edit_password_dialog.html.js';

export interface EditPasswordDialogElement {
  $: {
    dialog: CrDialogElement,
    saveButton: CrButtonElement,
    cancelButton: CrButtonElement,
    usernameInput: CrInputElement,
    passwordInput: CrInputElement,
    showPasswordButton: CrIconButtonElement,
  };
}

const EditPasswordDialogElementBase =
    ShowPasswordMixin(I18nMixin(PolymerElement));

export class EditPasswordDialogElement extends EditPasswordDialogElementBase {
  static get is() {
    return 'edit-password-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      password: Object,
    };
  }

  password: chrome.passwordsPrivate.PasswordUiEntry;

  private onCancel_() {
    this.$.dialog.close();
  }

  private getFootnote_(): string {
    assert(this.password.affiliatedDomains);
    return this.i18n(
        'editPasswordFootnote', this.password.affiliatedDomains[0]?.name ?? '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'edit-password-dialog': EditPasswordDialogElement;
  }
}

customElements.define(EditPasswordDialogElement.is, EditPasswordDialogElement);
