// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passkey-edit-dialog' is the dialog that allows showing or
 * editing a passkey.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './passkey_edit_dialog.css.js';
import {getHtml} from './passkey_edit_dialog.html.js';

export interface PasskeyEditDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const PasskeyEditDialogElementBase = I18nMixinLit(CrLitElement);

export type SavedPasskeyEditedEvent = CustomEvent<string>;

declare global {
  interface HTMLElementEventMap {
    'saved-passkey-edited': SavedPasskeyEditedEvent;
  }
}

export class PasskeyEditDialogElement extends PasskeyEditDialogElementBase {
  static get is() {
    return 'passkey-edit-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      username: {type: String},
      relyingPartyId: {type: String},
      usernameInputErrorMessage_: {type: String},
      dialogFootnote_: {type: String},
      usernameInputInvalid_: {type: Boolean},
    };
  }

  accessor username: string = '';
  accessor relyingPartyId: string = '';
  protected accessor usernameInputInvalid_: boolean = false;
  protected accessor usernameInputErrorMessage_: string = '';
  protected accessor dialogFootnote_: string = '';

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('relyingPartyId')) {
      this.dialogFootnote_ =
          this.i18n('passkeyEditDialogFootnote', this.relyingPartyId);
    }
    if (changedProperties.has('username')) {
      this.usernameInputInvalid_ = this.computeUsernameInputInvalid_();
    }
  }

  protected onUsernameValueChanged_(e: CustomEvent<{value: string}>) {
    this.username = e.detail.value;
  }

  protected onSaveButtonClick_() {
    this.fire('saved-passkey-edited', this.username);
    this.close();
  }

  private computeUsernameInputInvalid_(): boolean {
    if (this.username.length === 0) {
      this.usernameInputErrorMessage_ = this.i18n('passkeyLengthError');
      return true;
    }
    return false;
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  close() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passkey-edit-dialog': PasskeyEditDialogElement;
  }
}

customElements.define(PasskeyEditDialogElement.is, PasskeyEditDialogElement);
