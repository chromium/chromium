// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../shared_style.css.js';
import '../user_utils_mixin.js';
import './password_preview_item.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {MoveToAccountStoreTrigger, recordMoveToAccountStoreAccepted, recordMoveToAccountStoreOffered} from '../sharing/metrics_utils.js';
import {UserUtilMixin} from '../user_utils_mixin.js';

import {getTemplate} from './move_passwords_dialog.html.js';
import type {PasswordPreviewItemElement} from './password_preview_item.js';

export interface MovePasswordsDialogElement {
  $: {
    acceptButton: CrButtonElement,
    accountEmail: HTMLElement,
    dialog: CrDialogElement,
  };
}

const MovePasswordsDialogElementBase =
    WebUiListenerMixin(UserUtilMixin(I18nMixin(PolymerElement)));

export class MovePasswordsDialogElement extends MovePasswordsDialogElementBase {
  static get is() {
    return 'move-passwords-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Passwords group displayed in the UI.
       */
      passwords: {
        type: Array,
        value: () => [],
        observer: 'onPasswordsChanged_',
      },
      url: {type: String},
      hasOnlyOneDeviceCredential: {type: Boolean, value: true},

      selectedPasswordIds_: {
        type: Array,
        value: () => [],
      },
      descriptionString: {type: String},
      passwordsTitle: {type: String},
      trigger_: {
        type: Object,
        computed: 'computeTrigger_(passwords)',
      },
    };
  }

  declare passwords: chrome.passwordsPrivate.PasswordUiEntry[];
  declare private url: string;
  declare hasOnlyOneDeviceCredential: boolean;
  declare private selectedPasswordIds_: number[];
  declare descriptionString: string;
  declare passwordsTitle: string;
  declare private trigger_: MoveToAccountStoreTrigger;

  override connectedCallback() {
    super.connectedCallback();
    assert(loadTimeData.getBoolean('passwordUploadUiUpdate'));

    this.selectedPasswordIds_ = this.passwords.map(item => item.id);
    PasswordManagerImpl.getInstance()
        .requestCredentialsDetails(this.selectedPasswordIds_)
        .then(entries => {
          this.passwords = entries;
          recordMoveToAccountStoreOffered(this.trigger_);
          this.$.dialog.showModal();
        })
        .catch(() => {
          this.$.dialog.close();
        });
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onAcceptButtonClick_() {
    assert(this.isAccountStoreUser);

    PasswordManagerImpl.getInstance().movePasswordsToAccount(
        this.selectedPasswordIds_);
    this.dispatchEvent(new CustomEvent('passwords-moved', {
      bubbles: true,
      composed: true,
      detail: {
        accountEmail: this.accountEmail,
        numberOfPasswords: this.selectedPasswordIds_.length,
      },
    }));
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
    this.passwordsTitle = passwordsTitle.replace('$1', this.url);
  }

  private onPasswordsChanged_() {
    if (this.passwords.length === 0) {
      this.$.dialog.close();
      return;
    }

    this.updateDescriptionString_();
    this.updatePasswordsTitle_();
  }

  private onPasswordPreviewItemChange_() {
    this.selectedPasswordIds_ =
        Array
            .from(this.shadowRoot!.querySelectorAll<PasswordPreviewItemElement>(
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
