// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './shared_style.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './full_data_reset.html.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

export interface FullDataResetElement {
  $: {
    deleteAllButton: CrButtonElement,
    cancelButton: CrButtonElement,
    confirmButton: CrButtonElement,
    dialog: CrDialogElement,
    successToast: CrToastElement,
  };
}

const FullDataResetElementBase = I18nMixin(PolymerElement);

export class FullDataResetElement extends FullDataResetElementBase {
  static get is() {
    return 'full-data-reset';
  }

  static get template() {
    return getTemplate();
  }

  private onDeleteAllClick_(): void {
    this.$.dialog.showModal();
  }

  private onCancel_(): void {
    this.$.dialog.close();
  }

  private async onConfirm_() {
    this.$.dialog.close();
    const success =
        await PasswordManagerImpl.getInstance().deleteAllPasswordManagerData();
    this.showToastWithResult_(success);
  }

  private showToastWithResult_(success: boolean) {
    if (success) {
      this.$.successToast.show();
    } else {
      // TODO(crbug.com/342366264): Show error toast.
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'full-data-reset': FullDataResetElement;
  }
}

customElements.define(FullDataResetElement.is, FullDataResetElement);
