// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './metrics_utils.js';
import './share_password_dialog_header.js';
import './share_password_group_avatar.js';
import '../site_favicon.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasswordManagerProxy} from '../password_manager_proxy.js';
import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {UserUtilMixinLit} from '../user_utils_mixin_lit.js';

import {PasswordSharingActions, recordPasswordSharingInteraction} from './metrics_utils.js';
import {getCss} from './share_password_confirmation_dialog.css.js';
import {getHtml} from './share_password_confirmation_dialog.html.js';

export interface SharePasswordConfirmationDialogElement {
  $: {
    animation: HTMLElement,
    header: HTMLElement,
    cancel: HTMLElement,
    done: HTMLElement,
    dialog: CrDialogElement,
    senderAvatar: HTMLImageElement,
    recipientAvatar: HTMLElement,
    description: HTMLElement,
    footerDescription: HTMLElement,
  };
}

// Five seconds in milliseconds.
const FIVE_SECONDS = 5000;

export enum ConfirmationDialogStage {
  LOADING,
  CANCELED,
  SUCCESS,
}

const SharePasswordConfirmationDialogElementBase =
    UserUtilMixinLit(I18nMixinLit(CrLitElement));

export class SharePasswordConfirmationDialogElement extends
    SharePasswordConfirmationDialogElementBase {
  static get is() {
    return 'share-password-confirmation-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dialogStage_: {type: Number},
      password: {type: Object},
      passwordName: {type: String},
      iconUrl: {type: String},
      recipients: {type: Array},
    };
  }

  accessor password: chrome.passwordsPrivate.PasswordUiEntry;
  accessor passwordName: string = '';
  accessor iconUrl: string = '';
  accessor recipients: chrome.passwordsPrivate.RecipientInfo[] = [];
  protected accessor dialogStage_: ConfirmationDialogStage =
      ConfirmationDialogStage.LOADING;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    // Start the animation after all elements have been loaded.
    setTimeout(() => {
      this.$.animation.classList.add('loading');
    }, 0);

    // The user has 5 seconds to cancel the share action while loading/sharing
    // animation is in progress.
    setTimeout(() => {
      if (this.isStage_(ConfirmationDialogStage.CANCELED)) {
        return;
      }
      this.passwordManager_.sharePassword(this.password.id, this.recipients);
      this.dialogStage_ = ConfirmationDialogStage.SUCCESS;
    }, FIVE_SECONDS);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('dialogStage_')) {
      this.stateChange_();
    }
  }

  protected isStage_(stage: ConfirmationDialogStage): boolean {
    return this.dialogStage_ === stage;
  }

  private stateChange_() {
    // Don't reset focus on loading stage. Initially it is set correctly.
    if (this.isStage_(ConfirmationDialogStage.LOADING)) {
      return;
    }
    // Force the screen reader to focus on the updated dialog header.
    if (document.activeElement instanceof HTMLElement) {
      document.activeElement.blur();
    }

    this.$.dialog.focus();
  }


  protected getDialogTitle_(): string {
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

  protected getSuccessDescription_(): TrustedHTML {
    assert(this.recipients.length > 0);
    if (this.recipients.length > 1) {
      return this.i18nAdvanced(
          'sharePasswordConfirmationDescriptionMultipleRecipients', {
            substitutions: [
              this.passwordName,
            ],
          });
    }
    return this.i18nAdvanced(
        'sharePasswordConfirmationDescriptionSingleRecipient', {
          substitutions: [
            this.recipients[0].displayName,
            this.passwordName,
          ],
        });
  }

  private hasSecureChangePasswordUrl_(): boolean {
    const url = this.password.changePasswordUrl;
    return !!url && (url.startsWith('https://'));
  }

  protected getFooterDescription_(): TrustedHTML {
    // Only for Android Apps that don't have affiliated website, change password
    // url can't be generated.
    if (!this.password.changePasswordUrl) {
      return this.i18nAdvanced('sharePasswordConfirmationFooterAndroidApp');
    }

    // Don't insert change password url as '<a href>' for 'non-https' urls.
    return this.i18nAdvanced('sharePasswordConfirmationFooterWebsite', {
      substitutions: [
        this.hasSecureChangePasswordUrl_() ?
            `<a href='${this.password.changePasswordUrl}' target='_blank'>${
                this.passwordName}</a>` :
            this.passwordName,
      ],
    });
  }

  protected onFooterClick_(e: Event) {
    const element = e.target as HTMLElement;
    if (element.tagName === 'A') {
      recordPasswordSharingInteraction(
          PasswordSharingActions.CONFIRMATION_DIALOG_CHANGE_PASSWORD_CLICKED);
    }
  }

  protected onDoneClick_() {
    this.fire('close');
  }

  protected onCancelClick_() {
    // Ignore the click if a race with the 5 second timeout occurred.
    if (this.isStage_(ConfirmationDialogStage.SUCCESS)) {
      return;
    }
    recordPasswordSharingInteraction(
        PasswordSharingActions.CONFIRMATION_DIALOG_SHARING_CANCELED);
    this.dialogStage_ = ConfirmationDialogStage.CANCELED;
    this.$.animation.classList.remove('loading');
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
