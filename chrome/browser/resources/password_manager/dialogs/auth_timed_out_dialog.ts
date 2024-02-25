// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './auth_timed_out_dialog.html.js';

export interface AuthTimedOutDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const AuthTimedOutDialogElementBase = I18nMixin(PolymerElement);

export class AuthTimedOutDialogElement extends AuthTimedOutDialogElementBase {
  static get is() {
    return 'auth-timed-out-dialog';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private onCloseButtonClick_() {
    this.$.dialog.close();
  }

  private getTitle_(): string {
    return this.i18n('authTimedOut', this.i18n('localPasswordManager'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'auth-timed-out-dialog': AuthTimedOutDialogElement;
  }
}

customElements.define(AuthTimedOutDialogElement.is, AuthTimedOutDialogElement);
