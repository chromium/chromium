// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './metrics_utils.js';
import './share_password_dialog_header.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordSharingActions, recordPasswordSharingInteraction} from './metrics_utils.js';
import {getHtml} from './share_password_error_dialog.html.js';

export interface SharePasswordErrorDialogElement {
  $: {
    dialog: CrDialogElement,
    header: HTMLElement,
    description: HTMLElement,
    cancel: HTMLElement,
    tryAgain: HTMLElement,
  };
}

const SharePasswordErrorDialogElementBase = I18nMixinLit(CrLitElement);

export class SharePasswordErrorDialogElement extends
    SharePasswordErrorDialogElementBase {
  static get is() {
    return 'share-password-error-dialog';
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onCancelClick_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.ERROR_DIALOG_CANCELED);
    this.$.dialog.cancel();
  }

  protected onTryAgainClick_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.ERROR_DIALOG_TRY_AGAIN_CLICKED);
    this.fire('restart');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-error-dialog': SharePasswordErrorDialogElement;
  }
}

customElements.define(
    SharePasswordErrorDialogElement.is, SharePasswordErrorDialogElement);
