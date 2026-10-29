// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';

import {getCss} from './delete_passkey_dialog.css.js';
import {getHtml} from './delete_passkey_dialog.html.js';

export interface DeletePasskeyDialogElement {
  $: {
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    deleteButton: CrButtonElement,
  };
}

const DeletePasskeyDialogElementBase = I18nMixinLit(CrLitElement);

export class DeletePasskeyDialogElement extends DeletePasskeyDialogElementBase {
  static get is() {
    return 'delete-passkey-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      passkey: {type: Object},
    };
  }

  accessor passkey: chrome.passwordsPrivate.PasswordUiEntry;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('passkey')) {
      assert(this.passkey.isPasskey);
    }
  }

  protected onCancelClick_() {
    this.$.dialog.close();
  }

  protected onDeleteClick_() {
    PasswordManagerImpl.getInstance().removeCredential(
        this.passkey.id, this.passkey.storedIn);
    this.fire('passkey-removed');
    this.$.dialog.close();
  }

  protected getDescriptionHtml_(): TrustedHTML {
    // Passkeys have a single https affiliated origin corresponding to the
    // relying party identifier.
    assert(this.passkey.affiliatedDomains);
    const domain = this.passkey.affiliatedDomains[0];
    assert(domain);
    return this.i18nAdvanced('deletePasskeyConfirmationDescription', {
      substitutions:
          [`<a href='${domain.url}' target='_blank'>${domain.name}</a>`],
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'delete-passkey-dialog': DeletePasskeyDialogElement;
  }
}

customElements.define(
    DeletePasskeyDialogElement.is, DeletePasskeyDialogElement);
