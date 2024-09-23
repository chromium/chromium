// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import '../shared_style.css.js';
import './credential_details_card.css.js';
import '../dialogs/edit_password_dialog.js';
import '../dialogs/multi_store_delete_password_dialog.js';
import '../sharing/share_password_flow.js';
import '../sharing/metrics_utils.js';
import '../dialogs/move_single_password_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {PasswordsMovedEvent, ValueCopiedEvent} from '../password_manager_app.js';
import {PasswordManagerImpl, PasswordViewPageInteractions} from '../password_manager_proxy.js';
import {PasswordSharingActions, recordPasswordSharingInteraction} from '../sharing/metrics_utils.js';
import {ShowPasswordMixin} from '../show_password_mixin.js';
import {UserUtilMixin} from '../user_utils_mixin.js';

import type {CredentialFieldElement} from './credential_field.js';
import type {CredentialNoteElement} from './credential_note.js';
import {getTemplate} from './password_details_card.html.js';

export const PASSWORD_SHARE_BUTTON_BUTTON_ELEMENT_ID =
    'PasswordManagerUI::kSharePasswordElementId';

export type PasswordRemovedEvent =
    CustomEvent<{removedFromStores: chrome.passwordsPrivate.PasswordStoreSet}>;

declare global {
  interface HTMLElementEventMap {
    'password-removed': PasswordRemovedEvent;
    'passwords-moved': PasswordsMovedEvent;
    'value-copied': ValueCopiedEvent;
  }
}

export interface PasswordDetailsCardElement {
  $: {
    copyPasswordButton: CrIconButtonElement,
    deleteButton: CrButtonElement,
    domainLabel: HTMLElement,
    editButton: CrButtonElement,
    passwordValue: CrInputElement,
    noteValue: CredentialNoteElement,
    showMore: HTMLAnchorElement,
    showPasswordButton: CrIconButtonElement,
    usernameValue: CredentialFieldElement,
    shareButton: CrButtonElement,
    shareButtonContainer: HTMLElement,
  };
}

const PasswordDetailsCardElementBase = PrefsMixin(HelpBubbleMixin(
    UserUtilMixin(ShowPasswordMixin(I18nMixin(PolymerElement)))));

export class PasswordDetailsCardElement extends PasswordDetailsCardElementBase {
  static get is() {
    return 'password-details-card';
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
      groupName: String,
      iconUrl: String,
      usernameCopyInteraction_: {
        type: PasswordViewPageInteractions,
        value() {
          return PasswordViewPageInteractions.USERNAME_COPY_BUTTON_CLICKED;
        },
      },

      showEditPasswordDialog_: Boolean,
      showDeletePasswordDialog_: Boolean,
      showMovePasswordDialog_: Boolean,


      showShareButton_: {
        type: Boolean,
        value: false,
        // <if expr="_google_chrome">
        computed: 'computeShowShareButton_(isAccountStoreUser, ' +
            'isSyncingPasswords)',
        // </if>
      },

      passwordSharingDisabled_: {
        type: Boolean,
        computed: 'computePasswordSharingDisabled_(' +
            'prefs.password_manager.password_sharing_enabled.enforcement, ' +
            'prefs.password_manager.password_sharing_enabled.value)',
      },

      showShareFlow_: {
        type: Boolean,
        value: false,
      },

      isUsingAccountStore: Boolean,
    };
  }

  password: chrome.passwordsPrivate.PasswordUiEntry;
  groupName: string;
  iconUrl: string;
  isUsingAccountStore: boolean;
  private showEditPasswordDialog_: boolean;
  private passwordSharingDisabled_: boolean;
  private showDeletePasswordDialog_: boolean;
  private showShareFlow_: boolean;
  private showShareButton_: boolean;
  private showMovePasswordDialog_: boolean;

  private isFederated_(): boolean {
    return !!this.password.federationText;
  }

  private getPasswordLabel_() {
    return this.isFederated_() ? this.i18n('federationLabel') :
                                 this.i18n('passwordLabel');
  }

  private getPasswordValue_(): string|undefined {
    return this.isFederated_() ? this.password.federationText :
                                 this.password.password;
  }

  private getPasswordType_(): string {
    return this.isFederated_() ? 'text' : this.getPasswordInputType();
  }

  private onCopyPasswordClick_() {
    PasswordManagerImpl.getInstance()
        .requestPlaintextPassword(
            this.password.id, chrome.passwordsPrivate.PlaintextReason.COPY)
        .then(() => this.showToast_(this.i18n('passwordCopiedToClipboard')))
        .catch(() => {});
    this.extendAuthValidity_();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_COPY_BUTTON_CLICKED);
  }

  private onShowPasswordClick_() {
    this.onShowHidePasswordButtonClick();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_SHOW_BUTTON_CLICKED);
  }

  private onDeleteClick_() {
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_DELETE_BUTTON_CLICKED);
    if (this.password.storedIn ===
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT) {
      this.showDeletePasswordDialog_ = true;
      return;
    }
    PasswordManagerImpl.getInstance().removeCredential(
        this.password.id, this.password.storedIn);
    this.dispatchEvent(new CustomEvent('password-removed', {
      bubbles: true,
      composed: true,
      detail: {
        removedFromStores: this.password.storedIn,
      },
    }));
  }

  private showToast_(message: string) {
    this.dispatchEvent(new CustomEvent('value-copied', {
      bubbles: true,
      composed: true,
      detail: {toastMessage: message},
    }));
  }

  private onEditClicked_() {
    this.showEditPasswordDialog_ = true;
    this.extendAuthValidity_();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_EDIT_BUTTON_CLICKED);
  }

  private onEditPasswordDialogClosed_() {
    // Only note is notified because updating username or password triggers
    // recomputing of an id which updates the whole list of displayed passwords.
    this.notifyPath('password.note');
    this.showEditPasswordDialog_ = false;
    this.extendAuthValidity_();
  }

  private onDeletePasswordDialogClosed_() {
    this.showDeletePasswordDialog_ = false;
    this.extendAuthValidity_();
  }

  private onShareButtonClick_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.PASSWORD_DETAILS_SHARE_BUTTON_CLICKED);
    this.hideHelpBubble(PASSWORD_SHARE_BUTTON_BUTTON_ELEMENT_ID);
    this.showShareFlow_ = true;
  }

  private onShareFlowDone_() {
    this.showShareFlow_ = false;
    setTimeout(() => {
      this.$.shareButton.focus();
    }, 0);
  }

  private extendAuthValidity_() {
    PasswordManagerImpl.getInstance().extendAuthValidity();
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

  private computeShowShareButton_(): boolean {
    return !this.isFederated_() &&
        (this.isSyncingPasswords || this.isAccountStoreUser);
  }

  private computePasswordSharingDisabled_(): boolean {
    const pref = this.getPref('password_manager.password_sharing_enabled');
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED &&
        !pref.value;
  }

  private getCredentialTypeString_(): string {
    return this.isFederated_() ? this.i18n(
                                     'federatedCredentialProviderAriaLabel',
                                     this.password.federationText!) :
                                 this.i18n('passwordLabel');
  }

  private getAriaLabelForPasswordCard_(): string {
    return this.password.username ?
        this.i18n(
            'passwordDetailsCardAriaLabel', this.getCredentialTypeString_(),
            this.password.username) :
        this.getCredentialTypeString_();
  }

  private getAriaLabelForEditButton_(): string {
    return this.password.username ?
        this.i18n(
            'passwordDetailsCardEditButtonAriaLabel',
            this.getCredentialTypeString_(), this.password.username) :
        this.i18n(
            'passwordDetailsCardEditButtonNoUsernameAriaLabel',
            this.getCredentialTypeString_());
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

  private computeMovePasswordText_(): TrustedHTML {
    return this.i18nAdvanced('moveSinglePassword');
  }

  private movePasswordClicked_(e: Event): void {
    e.preventDefault();
    this.showMovePasswordDialog_ = true;
  }

  private showMovePasswordEntry_(): boolean {
    return this.isUsingAccountStore &&
        this.password.storedIn ===
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
  }

  private onMovePasswordDialogClose_(): void {
    this.showMovePasswordDialog_ = false;
  }

  private onPasswordChanged_(): void {
    this.isPasswordVisible = false;
  }

  maybeRegisterSharingHelpBubble(): void {
    if (!this.showShareButton_ && !this.passwordSharingDisabled_) {
      return;
    }

    this.registerHelpBubble(
        PASSWORD_SHARE_BUTTON_BUTTON_ELEMENT_ID, this.$.shareButtonContainer);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-details-card': PasswordDetailsCardElement;
  }
}

customElements.define(
    PasswordDetailsCardElement.is, PasswordDetailsCardElement);
