// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../i18n_setup.js';
import './passwords_shared.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordCheckInteraction, PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {getTemplate} from './password_remove_confirmation_dialog.html.js';

export interface SettingsPasswordRemoveConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
    remove: CrButtonElement,
    text: HTMLElement,
    link: HTMLElement,
  };
}

const SettingsPasswordRemoveConfirmationDialogElementBase =
    I18nMixin(PolymerElement);

export class SettingsPasswordRemoveConfirmationDialogElement extends
    SettingsPasswordRemoveConfirmationDialogElementBase {
  static get is() {
    return 'settings-password-remove-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The password that is being displayed.
       */
      item: Object,
    };
  }

  item: chrome.passwordsPrivate.PasswordUiEntry;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private onRemoveClick_() {
    this.passwordManager_.recordPasswordCheckInteraction(
        PasswordCheckInteraction.REMOVE_PASSWORD);
    this.passwordManager_.removeCredential(this.item.id, this.item.storedIn);
    this.$.dialog.close();
  }

  private onCancelClick_() {
    this.$.dialog.close();
  }

  private hasSecureChangePasswordUrl_(): boolean {
    const url = this.item.changePasswordUrl;
    return !!url && (url.startsWith('https://') || url.startsWith('chrome://'));
  }

  /**
   * Returns the remove password description with a linkified change password
   * URL. Requires the change password URL to be present and secure.
   */
  private getRemovePasswordDescriptionHtml_(): TrustedHTML {
    if (!this.hasSecureChangePasswordUrl_()) {
      return window.trustedTypes!.emptyHTML;
    }

    const url: string|undefined = this.item.changePasswordUrl;
    assert(url);
    const origin = this.item.urls.shown;
    return this.i18nAdvanced(
        'removeCompromisedPasswordConfirmationDescription', {
          substitutions:
              [origin, `<a href='${url}' target='_blank'>${origin}</a>`],
        });
  }

  /**
   * Returns the remove password description as a plain text.
   * Used when the change password URL is not present or insecure.
   */
  private getRemovePasswordDescriptionText_(): string {
    const origin = this.item.urls.shown;
    return this.i18n(
        'removeCompromisedPasswordConfirmationDescription', origin, origin);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-password-remove-confirmation-dialog':
        SettingsPasswordRemoveConfirmationDialogElement;
  }
}
customElements.define(
    SettingsPasswordRemoveConfirmationDialogElement.is,
    SettingsPasswordRemoveConfirmationDialogElement);
