// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './credential_field.js';
import '../dialogs/edit_passkey_dialog.js';
import '../dialogs/delete_passkey_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {htmlEscape} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl, PasswordViewPageInteractions} from '../password_manager_proxy.js';

import type {CredentialFieldElement} from './credential_field.js';
import {getCss} from './passkey_details_card.css.js';
import {getHtml} from './passkey_details_card.html.js';

export interface PasskeyDetailsCardElement {
  $: {
    deleteButton: CrButtonElement,
    domainLabel: HTMLElement,
    editButton: CrButtonElement,
    usernameValue: CredentialFieldElement,
    displayNameValue: CredentialFieldElement,
    infoLabelContent: HTMLElement,
  };
}

const PasskeyDetailsCardElementBase = I18nMixinLit(CrLitElement);

export class PasskeyDetailsCardElement extends PasskeyDetailsCardElementBase {
  static get is() {
    return 'passkey-details-card';
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
      showEditPasskeyDialog_: {type: Boolean},
      showDeletePasskeyDialog_: {type: Boolean},
    };
  }

  accessor passkey: chrome.passwordsPrivate.PasswordUiEntry;
  protected accessor showEditPasskeyDialog_: boolean = false;
  protected accessor showDeletePasskeyDialog_: boolean = false;

  protected getUsernameValue_(): string {
    return !this.passkey.username || this.passkey.username === '' ?
        this.i18n('usernamePlaceholder') :
        this.passkey.username;
  }

  protected getDisplayNameValue_(): string {
    return !this.passkey.displayName || this.passkey.displayName === '' ?
        this.i18n('displayNamePlaceholder') :
        this.passkey.displayName;
  }

  protected onDeleteClick_() {
    this.showDeletePasskeyDialog_ = true;
    PasswordManagerImpl.getInstance().extendAuthValidity();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSKEY_DELETE_BUTTON_CLICKED);
  }

  protected onDeletePasskeyDialogClose_() {
    this.showDeletePasskeyDialog_ = false;
    PasswordManagerImpl.getInstance().extendAuthValidity();
  }

  protected onEditClick_() {
    this.showEditPasskeyDialog_ = true;
    PasswordManagerImpl.getInstance().extendAuthValidity();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSKEY_EDIT_BUTTON_CLICKED);
  }

  protected onEditPasskeyDialogClose_() {
    this.showEditPasskeyDialog_ = false;
    PasswordManagerImpl.getInstance().extendAuthValidity();
  }

  protected getAriaLabelForPasswordCard_(): string {
    return !this.passkey.username ?
        this.i18n('passkeyDetailsCardNoUsernameAriaLabel') :
        this.i18n(
            'passkeyDetailsCardAriaLabel',
            htmlEscape(this.passkey.username));
  }

  protected getAriaLabelForEditButton_(): string {
    return !this.passkey.username ?
        this.i18n('passkeyDetailsCardEditButtonNoUsernameAriaLabel') :
        this.i18n(
            'passkeyDetailsCardEditButtonAriaLabel',
            htmlEscape(this.passkey.username));
  }

  protected getAriaLabelForDeleteButton_(): string {
    return !this.passkey.username ?
        this.i18n('passkeyDetailsCardDeleteButtonNoUsernameAriaLabel') :
        this.i18n(
            'passkeyDetailsCardDeleteButtonAriaLabel',
            htmlEscape(this.passkey.username));
  }

  protected getInfoLabelText_() {
    if (this.passkey.hidden) {
      return this.i18nAdvanced('passkeyHiddenInfoLabel', {
        substitutions: [this.passkey.affiliatedDomains[0].name],
        tags: ['a'],
      });
    }

    // Google Password Manager passkeys always have their creation time
    // available.
    assert(this.passkey.creationTime !== undefined);

    const date = new Date(this.passkey.creationTime);
    return this.i18nAdvanced('passkeyManagementInfoLabel', {
      substitutions: [date.toLocaleDateString(/*locales=*/ undefined,
                                              {dateStyle: 'short'})],
    });
  }

  protected getPasskeyIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:passkey' :
        'passwords-icon:passkey-old';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passkey-details-card': PasskeyDetailsCardElement;
  }
}

customElements.define(PasskeyDetailsCardElement.is, PasskeyDetailsCardElement);
