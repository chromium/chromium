// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {htmlEscape} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasswordManagerProxy} from '../password_manager_proxy.js';
import {PasswordCheckInteraction, PasswordManagerImpl} from '../password_manager_proxy.js';

import {getCss} from './delete_password_disclaimer_dialog.css.js';
import {getHtml} from './delete_password_disclaimer_dialog.html.js';

export interface DeletePasswordDisclaimerDialogElement {
  $: {
    dialog: CrDialogElement,
    delete: CrButtonElement,
    text: HTMLElement,
    link: HTMLElement,
  };
}

const DeletePasswordDisclaimerDialogElementBase = I18nMixinLit(CrLitElement);

export class DeletePasswordDisclaimerDialogElement extends
    DeletePasswordDisclaimerDialogElementBase {
  static get is() {
    return 'delete-password-disclaimer-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The password that is being displayed.
       */
      origin: {type: String},
      actionUrl: {type: String},
    };
  }

  accessor origin: string = '';
  accessor actionUrl: string = '';

  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  protected onDeleteClick_() {
    this.fire('delete-password-click');
    this.passwordManager_.recordPasswordCheckInteraction(
        PasswordCheckInteraction.REMOVE_PASSWORD);
    this.$.dialog.close();
  }

  protected onCancelClick_() {
    this.$.dialog.close();
  }

  protected hasSecureChangePasswordUrl_(): boolean {
    const url = this.actionUrl;
    return !!url && (url.startsWith('https://'));
  }

  /**
   * Returns the delete password description with a linkified change password
   * URL. Requires the change password URL to be present and secure.
   */
  protected getDescriptionHtml_(): TrustedHTML {
    if (!this.hasSecureChangePasswordUrl_()) {
      return window.trustedTypes!.emptyHTML;
    }

    const escapedOrigin = htmlEscape(this.origin ?? '');
    return this.i18nAdvanced('deletePasswordConfirmationDescription', {
      substitutions: [
        escapedOrigin,
        `<a href='${this.actionUrl}' target='_blank'>${escapedOrigin}</a>`,
      ],
    });
  }

  /**
   * Returns the delete password description as a plain text.
   * Used when the change password URL is not present or insecure.
   */
  protected getDescriptionText_(): string {
    return this.i18n(
        'deletePasswordConfirmationDescription', this.origin, this.origin);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'delete-password-disclaimer-dialog': DeletePasswordDisclaimerDialogElement;
  }
}
customElements.define(
    DeletePasswordDisclaimerDialogElement.is,
    DeletePasswordDisclaimerDialogElement);
