// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './share_password_dialog_header.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from '../password_manager_proxy.js';

import {getTemplate} from './share_password_confirmation_dialog.html.js';

export interface SharePasswordConfirmationDialogElement {
  $: {
    header: HTMLElement,
    cancel: HTMLElement,
    done: HTMLElement,
  };
}

// Five seconds in milliseconds.
const FIVE_SECONDS = 5000;

enum ConfirmationDialogStage {
  LOADING,
  CANCELED,
  SUCCESS,
}

export class SharePasswordConfirmationDialogElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'share-password-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dialogStage_: Number,

      passwordId: Number,

      recipients: {
        type: Array,
        value: [],
      },

      dialogStageEnum_: {
        type: Object,
        value: ConfirmationDialogStage,
        readOnly: true,
      },
    };
  }

  recipients: chrome.passwordsPrivate.RecipientInfo[];
  passwordId: number;
  private dialogStage_: ConfirmationDialogStage =
      ConfirmationDialogStage.LOADING;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override ready() {
    super.ready();

    // The user has 5 seconds to cancel the share action while loading/sharing
    // animation is in progress.
    setTimeout(() => {
      if (this.isStage_(ConfirmationDialogStage.CANCELED)) {
        return;
      }
      this.passwordManager_.sharePassword(this.passwordId, this.recipients);
      this.dialogStage_ = ConfirmationDialogStage.SUCCESS;
    }, FIVE_SECONDS);
  }

  private isStage_(stage: ConfirmationDialogStage): boolean {
    return this.dialogStage_ === stage;
  }


  private getDialogTitle_(): string {
    switch (this.dialogStage_) {
      case ConfirmationDialogStage.LOADING:
        return this.i18n('shareDialogLoadingTitle');
      case ConfirmationDialogStage.CANCELED:
        return this.i18n('shareDialogCanceledTitle');
      case ConfirmationDialogStage.SUCCESS:
        return this.i18n('shareDialogSuccessTitle');
      default:
        assertNotReached();
    }
  }

  private onClickDone_() {
    this.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));
  }

  private onClickCancel_() {
    // Ignore the click if a race with the 5 second timeout occurred.
    if (this.isStage_(ConfirmationDialogStage.SUCCESS)) {
      return;
    }
    this.dialogStage_ = ConfirmationDialogStage.CANCELED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-confirmation-dialog':
        SharePasswordConfirmationDialogElement;
  }
}

customElements.define(
    SharePasswordConfirmationDialogElement.is,
    SharePasswordConfirmationDialogElement);
