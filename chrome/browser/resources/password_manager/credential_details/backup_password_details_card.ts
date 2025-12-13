// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_input/cr_input_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import '../shared_style.css.js';
import './credential_details_card.css.js';
import './credential_field.js';
import './credential_note.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ValueCopiedEvent} from '../password_manager_app.js';
import {PasswordManagerImpl, PasswordViewPageInteractions} from '../password_manager_proxy.js';
import {ShowPasswordMixin} from '../show_password_mixin.js';

import {getTemplate} from './backup_password_details_card.html.js';
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
    showMore: HTMLAnchorElement,
    showPasswordButton: CrIconButtonElement,
    usernameValue: CredentialFieldElement,
  };
}

const BackupPasswordDetailsCardElementBase =
    ShowPasswordMixin(I18nMixin(PolymerElement));

export class BackupPasswordDetailsCardElement extends
    BackupPasswordDetailsCardElementBase {
  static get is() {
    return 'backup-password-details-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      password: {
        type: Object,
        observer: 'onPasswordChanged_',
      },

      interactionsEnum_: {
        type: Object,
        value: PasswordViewPageInteractions,
      },
    };
  }

  declare password: chrome.passwordsPrivate.PasswordUiEntry;

  private getNoteValue_(): string|undefined {
    assert(this.password.backupPassword);
    const brandingName = this.i18n('localPasswordManager');
    const detailsHeading = this.i18n('passwordDetailsCardBackupPasswordNote');
    const detailsContent = this.i18n(
        'passwordDetailsCardBackupPasswordNoteDetails',
        this.password.backupPassword.creationDate, brandingName);

    return `${detailsHeading}\n\n${detailsContent}`;
  }

  private onCopyPasswordClick_() {
    PasswordManagerImpl.getInstance()
        .copyPlaintextBackupPassword(this.password.id)
        .then(() => this.showToast_(this.i18n('passwordCopiedToClipboard')))
        .catch(() => {});
    PasswordManagerImpl.getInstance().extendAuthValidity();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_COPY_BUTTON_CLICKED);
  }

  private onShowPasswordClick_() {
    this.onShowHidePasswordButtonClick();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_SHOW_BUTTON_CLICKED);
  }

  private onDeleteClick_() {
    PasswordManagerImpl.getInstance().removeBackupPassword(this.password.id);
    this.dispatchEvent(new CustomEvent('backup-password-removed', {
      bubbles: true,
      composed: true,
    }));
    return;
  }

  private getDomainLabel_(): string {
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
    this.dispatchEvent(new CustomEvent('value-copied', {
      bubbles: true,
      composed: true,
      detail: {toastMessage: message},
    }));
  }


  private getCredentialTypeString_(): string {
    return this.i18n('passwordLabel');
  }

  private getAriaLabelForBackupPasswordCard_(): string {
    return this.password.username ?
        this.i18n(
            'backupPasswordDetailsCardAriaLabel',
            this.getCredentialTypeString_(), this.password.username) :
        this.getCredentialTypeString_();
  }

  private getAriaLabelForDeleteButton_(): string {
    return this.password.username ?
        this.i18n(
            'passwordDetailsCardDeleteButtonAriaLabel',
            this.getCredentialTypeString_(), this.password.username) :
        this.i18n(
            'passwordDetailsCardDeleteButtonNoUsernameAriaLabel',
            this.getCredentialTypeString_());
  }

  private onPasswordChanged_(): void {
    this.isPasswordVisible = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'backup-password-details-card': BackupPasswordDetailsCardElement;
  }
}

customElements.define(
    BackupPasswordDetailsCardElement.is, BackupPasswordDetailsCardElement);
