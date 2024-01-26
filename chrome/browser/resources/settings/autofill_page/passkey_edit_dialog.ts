// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passkey-edit-dialog' is the dialog that allows showing or
 * editing a passkey.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../icons.html.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import './passwords_shared.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './passkey_edit_dialog.html.js';

export interface PasskeyEditDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const PasskeyEditDialogElementBase = I18nMixin(PolymerElement);

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

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      username: String,
      relyingPartyId: String,
      usernameInputErrorMessage_: String,
      dialogFootnote_: String,
      usernameInputInvalid_: {
        type: Boolean,
        computed: 'computeUsernameInputInvalid_(username)',
      },
    };
  }

  username: string;
  relyingPartyId: string;
  private usernameInputInvalid_: boolean;
  private usernameInputErrorMessage_: string|null;
  private dialogFootnote_: string|null;

  override ready() {
    super.ready();
    this.dialogFootnote_ =
        this.i18n('passkeyEditDialogFootnote', this.relyingPartyId);
  }

  private onSaveButtonClick_() {
    this.dispatchEvent(new CustomEvent('saved-passkey-edited', {
      bubbles: true,
      composed: true,
      detail: this.username,
    }));
    this.close();
  }

  private computeUsernameInputInvalid_(): boolean {
    if (this.username.length === 0) {
      this.usernameInputErrorMessage_ = this.i18n('passkeyLengthError');
      return true;
    }
    return false;
  }

  private onCancel_() {
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
