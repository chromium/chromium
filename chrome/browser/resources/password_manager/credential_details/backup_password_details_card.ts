// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import './credential_field.js';
import './credential_note.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ValueCopiedEvent} from '../password_manager_app.js';
import {PasswordManagerImpl, PasswordViewPageInteractions} from '../password_manager_proxy.js';
import {ShowPasswordMixinLit} from '../show_password_mixin_lit.js';

import {getCss} from './backup_password_details_card.css.js';
import {getHtml} from './backup_password_details_card.html.js';
import type {CredentialFieldElement} from './credential_field.js';
import type {CredentialNoteElement} from './credential_note.js';

export type BackupPasswordRemovedEvent = CustomEvent<{}>;

declare global {
  interface HTMLElementEventMap {
    'value-copied': ValueCopiedEvent;
    'backup-password-removed': BackupPasswordRemovedEvent;
  }
}

export interface BackupPasswordDetailsCardElement {
  $: {
    copyPasswordButton: CrIconButtonElement,
    deleteButton: CrButtonElement,
    domainLabel: HTMLElement,
    passwordValue: CrInputElement,
    noteValue: CredentialNoteElement,
    showPasswordButton: CrIconButtonElement,
    usernameValue: CredentialFieldElement,
  };
}

const BackupPasswordDetailsCardElementBase =
    ShowPasswordMixinLit(I18nMixinLit(CrLitElement));

export class BackupPasswordDetailsCardElement extends
    BackupPasswordDetailsCardElementBase {
  static get is() {
    return 'backup-password-details-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      password: {type: Object},
    };
  }

  accessor password: chrome.passwordsPrivate.PasswordUiEntry;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('password')) {
      this.isPasswordVisible = false;
    }
  }

  protected getNoteValue_(): string {
    assert(this.password.backupPassword);
    const brandingName = this.i18n('localPasswordManager');
    const detailsHeading = this.i18n('passwordDetailsCardBackupPasswordNote');
    const detailsContent = this.i18n(
        'passwordDetailsCardBackupPasswordNoteDetails',
        this.password.backupPassword.creationDate, brandingName);

    return `${detailsHeading}\n\n${detailsContent}`;
  }

  protected onCopyPasswordClick_() {
    PasswordManagerImpl.getInstance()
        .copyPlaintextBackupPassword(this.password.id)
        .then(success => {
          if (success) {
            this.showToast_(this.i18n('passwordCopiedToClipboard'));
          }
        });
    PasswordManagerImpl.getInstance().extendAuthValidity();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_COPY_BUTTON_CLICKED);
  }

  protected onShowPasswordClick_() {
    this.onShowHidePasswordButtonClick();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_SHOW_BUTTON_CLICKED);
  }

  protected onDeleteClick_() {
    PasswordManagerImpl.getInstance().removeBackupPassword(this.password.id);
    this.fire('backup-password-removed');
    return;
  }

  protected getDomainLabel_(): string {
    const hasApps = this.password.affiliatedDomains?.some(
        domain => domain.signonRealm.startsWith('android://'));
    const hasSites = this.password.affiliatedDomains?.some(
        domain => !domain.signonRealm.startsWith('android://'));
    if (hasApps && hasSites) {
      return this.i18n('sitesAndAppsLabel');
    }
    return hasApps ? this.i18n('appsLabel') : this.i18n('sitesLabel');
  }

  private showToast_(message: string) {
    this.fire('value-copied', {toastMessage: message});
  }


  private getCredentialTypeString_(): string {
    return this.i18n('passwordLabel');
  }

  protected getAriaLabelForBackupPasswordCard_(): string {
    return this.password.username ?
        this.i18n(
            'backupPasswordDetailsCardAriaLabel',
            this.getCredentialTypeString_(), this.password.username) :
        this.getCredentialTypeString_();
  }

  protected getAriaLabelForDeleteButton_(): string {
    return this.password.username ?
        this.i18n(
            'passwordDetailsCardDeleteButtonAriaLabel',
            this.getCredentialTypeString_(), this.password.username) :
        this.i18n(
            'passwordDetailsCardDeleteButtonNoUsernameAriaLabel',
            this.getCredentialTypeString_());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'backup-password-details-card': BackupPasswordDetailsCardElement;
  }
}

customElements.define(
    BackupPasswordDetailsCardElement.is, BackupPasswordDetailsCardElement);
