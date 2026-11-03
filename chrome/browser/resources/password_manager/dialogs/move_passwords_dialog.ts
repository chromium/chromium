// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './password_preview_item.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {MoveToAccountStoreTrigger, recordMoveToAccountStoreAccepted, recordMoveToAccountStoreOffered} from '../sharing/metrics_utils.js';
import {UserUtilMixinLit} from '../user_utils_mixin_lit.js';

import {getCss} from './move_passwords_dialog.css.js';
import {getHtml} from './move_passwords_dialog.html.js';
import type {PasswordPreviewItemElement} from './password_preview_item.js';

export interface MovePasswordsDialogElement {
  $: {
    acceptButton: CrButtonElement,
    accountEmail: HTMLElement,
    dialog: CrDialogElement,
  };
}

const MovePasswordsDialogElementBase =
    WebUiListenerMixinLit(UserUtilMixinLit(I18nMixinLit(CrLitElement)));

export class MovePasswordsDialogElement extends MovePasswordsDialogElementBase {
  static get is() {
    return 'move-passwords-dialog';
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
       * Passwords group displayed in the UI.
       */
      passwords: {type: Array},
      url: {type: String},
      hasOnlyOneDeviceCredential: {type: Boolean},
      selectedPasswordIds_: {type: Array},
      descriptionString: {type: String},
      passwordsTitle_: {type: String},
    };
  }

  accessor passwords: chrome.passwordsPrivate.PasswordUiEntry[] = [];
  accessor url: string = '';
  accessor hasOnlyOneDeviceCredential: boolean = true;
  protected accessor selectedPasswordIds_: number[] = [];
  accessor descriptionString: string = '';
  protected accessor passwordsTitle_: string = '';
  private trigger_: MoveToAccountStoreTrigger =
      MoveToAccountStoreTrigger.EXPLICITLY_TRIGGERED_IN_SETTINGS;

  override connectedCallback() {
    this.selectedPasswordIds_ = this.passwords.map(item => item.id);
    super.connectedCallback();
    PasswordManagerImpl.getInstance()
        .requestCredentialsDetails(this.selectedPasswordIds_)
        .then(async entries => {
          this.passwords = entries;
          await this.updateComplete;
          recordMoveToAccountStoreOffered(this.trigger_);
          this.$.dialog.showModal();
        })
        .catch(() => {
          this.$.dialog.close();
        });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('passwords')) {
      this.trigger_ = this.computeTrigger_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('passwords')) {
      this.onPasswordsChanged_();
    }
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  protected onAcceptButtonClick_() {
    assert(this.isAccountStoreUser);

    PasswordManagerImpl.getInstance().movePasswordsToAccount(
        this.selectedPasswordIds_);
    this.fire('passwords-moved', {
      accountEmail: this.accountEmail,
      numberOfPasswords: this.selectedPasswordIds_.length,
    });
    recordMoveToAccountStoreAccepted(this.trigger_);

    this.$.dialog.close();
  }

  private async updateDescriptionString_() {
    const description = this.hasOnlyOneDeviceCredential ?
        this.i18n('moveSinglePasswordDialogDescription') :
        await PluralStringProxyImpl.getInstance().getPluralString(
            'movePasswordsDialogDescription', this.passwords.length);
    this.descriptionString = description.replace('$1', this.url);
  }

  private async updatePasswordsTitle_() {
    const passwordsTitle =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'movePasswordsDialogPasswordsTitle', this.passwords.length);
    this.passwordsTitle_ = passwordsTitle.replace('$1', this.url);
  }

  private onPasswordsChanged_() {
    if (this.passwords.length === 0) {
      this.$.dialog.close();
      return;
    }

    this.updateDescriptionString_();
    this.updatePasswordsTitle_();
  }

  protected onPasswordPreviewItemChange_() {
    this.selectedPasswordIds_ =
        Array
            .from(this.shadowRoot.querySelectorAll<PasswordPreviewItemElement>(
                'password-preview-item[checked]'))
            .map(item => item.passwordId);
  }

  private computeTrigger_() {
    return this.passwords.length > 1 ?
        MoveToAccountStoreTrigger
            .EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS :
        MoveToAccountStoreTrigger.EXPLICITLY_TRIGGERED_IN_SETTINGS;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'move-passwords-dialog': MovePasswordsDialogElement;
  }
}

customElements.define(
    MovePasswordsDialogElement.is, MovePasswordsDialogElement);
