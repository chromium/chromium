// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import './credential_field.js';
import './credential_note.js';
import '../dialogs/edit_password_dialog.js';
import '../dialogs/multi_store_delete_password_dialog.js';
import '../sharing/share_password_flow.js';
import '../sharing/metrics_utils.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {htmlEscape} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasswordsMovedEvent, ValueCopiedEvent} from '../password_manager_app.js';
import {PasswordManagerImpl, PasswordViewPageInteractions} from '../password_manager_proxy.js';
import {Page, Router} from '../router.js';
import {MoveToAccountStoreTrigger, PasswordSharingActions, recordMoveToAccountStoreAccepted, recordPasswordSharingInteraction} from '../sharing/metrics_utils.js';
import {ShowPasswordMixinLit} from '../show_password_mixin_lit.js';
import {UserUtilMixinLit} from '../user_utils_mixin_lit.js';

import type {CredentialFieldElement} from './credential_field.js';
import type {CredentialNoteElement} from './credential_note.js';
import {getCss} from './password_details_card.css.js';
import {getHtml} from './password_details_card.html.js';

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
    showPasswordButton: CrIconButtonElement,
    usernameValue: CredentialFieldElement,
    shareButton: CrButtonElement,
    shareButtonContainer: HTMLElement,
  };
}

const PasswordDetailsCardElementBase =
    PrefServiceObserverMixinLit(HelpBubbleMixinLit(
        UserUtilMixinLit(ShowPasswordMixinLit(I18nMixinLit(CrLitElement)))));

export class PasswordDetailsCardElement extends PasswordDetailsCardElementBase {
  static get is() {
    return 'password-details-card';
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
      groupName: {type: String},
      iconUrl: {type: String},
      shouldRegisterSharingPromo: {type: Boolean},
      showEditPasswordDialog_: {type: Boolean},
      showDeletePasswordDialog_: {type: Boolean},
      showShareButton_: {type: Boolean},
      passwordSharingDisabled_: {type: Boolean},
      showShareFlow_: {type: Boolean},
      isUsingAccountStore: {type: Boolean},
    };
  }

  accessor password: chrome.passwordsPrivate.PasswordUiEntry;
  accessor groupName: string;
  accessor iconUrl: string;
  accessor isUsingAccountStore: boolean = false;
  /* This is set by the parent element, to only show help buble on the first
   * card on the page. */
  accessor shouldRegisterSharingPromo: boolean = false;
  protected accessor showEditPasswordDialog_: boolean = false;
  protected accessor passwordSharingDisabled_: boolean = false;
  protected accessor showDeletePasswordDialog_: boolean = false;
  protected accessor showShareFlow_: boolean = false;
  protected accessor showShareButton_: boolean = false;

  private trigger_: MoveToAccountStoreTrigger =
      MoveToAccountStoreTrigger
          .EXPLICITLY_TRIGGERED_FOR_SINGLE_PASSWORD_IN_DETAILS_IN_SETTINGS;

  override connectedCallback() {
    super.connectedCallback();

    this.addPrefObserver<boolean>(
        'password_manager.password_sharing_enabled', pref => {
          this.passwordSharingDisabled_ = pref.enforcement ===
                  chrome.settingsPrivate.Enforcement.ENFORCED &&
              !pref.value;
        });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('password')) {
      this.isPasswordVisible = false;
    }

    // <if expr="_google_chrome">
    if (changedProperties.has('password') ||
        changedProperties.has('isAccountStoreUser') ||
        changedProperties.has('isSyncingPasswords')) {
      this.showShareButton_ = this.computeShowShareButton_();
    }
    // </if>
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedProperties.has('shouldRegisterSharingPromo') ||
        changedPrivateProperties.has('showShareButton_') ||
        changedPrivateProperties.has('passwordSharingDisabled_')) {
      this.maybeRegisterSharingHelpBubble_();
    }
  }

  protected isFederated_(): boolean {
    return !!this.password.federationText;
  }

  protected getPasswordLabel_() {
    return this.isFederated_() ? this.i18n('federationLabel') :
                                 this.i18n('passwordLabel');
  }

  protected getPasswordValue_(): string|undefined {
    if (this.isFederated_()) {
      return this.password.federationText;
    }
    return this.password.password;
  }

  protected getPasswordType_(): string {
    return this.isFederated_() ? 'text' : this.getPasswordInputType();
  }

  protected onCopyPasswordClick_() {
    PasswordManagerImpl.getInstance()
        .requestPlaintextPassword(
            this.password.id, chrome.passwordsPrivate.PlaintextReason.COPY)
        .then(() => this.showToast_(this.i18n('passwordCopiedToClipboard')))
        .catch(() => {});
    this.extendAuthValidity_();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_COPY_BUTTON_CLICKED);
  }

  protected onShowPasswordClick_() {
    this.onShowHidePasswordButtonClick();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_SHOW_BUTTON_CLICKED);
  }

  protected onDeleteClick_() {
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_DELETE_BUTTON_CLICKED);
    if (this.password.storedIn ===
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT) {
      this.showDeletePasswordDialog_ = true;
      return;
    }
    PasswordManagerImpl.getInstance().removeCredential(
        this.password.id, this.password.storedIn);
    this.fire('password-removed', {
      removedFromStores: this.password.storedIn,
    });
  }

  private showToast_(message: string) {
    this.fire('value-copied', {toastMessage: message});
  }

  protected onEditClick_() {
    this.showEditPasswordDialog_ = true;
    this.extendAuthValidity_();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_EDIT_BUTTON_CLICKED);
  }

  protected onEditPasswordDialogClose_() {
    this.showEditPasswordDialog_ = false;
    this.extendAuthValidity_();
  }

  protected onDeletePasswordDialogClose_() {
    this.showDeletePasswordDialog_ = false;
    this.extendAuthValidity_();
  }

  protected onShareButtonClick_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.PASSWORD_DETAILS_SHARE_BUTTON_CLICKED);
    this.hideHelpBubble(PASSWORD_SHARE_BUTTON_BUTTON_ELEMENT_ID);
    this.showShareFlow_ = true;
  }

  protected onShareFlowDone_() {
    this.showShareFlow_ = false;
    setTimeout(() => {
      this.$.shareButton.focus();
    }, 0);
  }

  private extendAuthValidity_() {
    PasswordManagerImpl.getInstance().extendAuthValidity();
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

  protected getMovePasswordsSubtitle_(): string {
    return this.i18n(
        'movePasswordsToAccountDetailsCardSubtitle', this.accountEmail);
  }

  private computeShowShareButton_(): boolean {
    return !this.isFederated_() &&
        (this.isSyncingPasswords || this.isAccountStoreUser);
  }

  private getCredentialTypeString_(): string {
    return this.isFederated_() ? this.i18n(
                                     'federatedCredentialProviderAriaLabel',
                                     this.password.federationText!) :
                                 this.i18n('passwordLabel');
  }

  protected getAriaLabelForPasswordCard_(): string {
    return this.password.username ?
        this.i18n(
            'passwordDetailsCardAriaLabel', this.getCredentialTypeString_(),
            htmlEscape(this.password.username)) :
        this.getCredentialTypeString_();
  }

  protected getAriaLabelForEditButton_(): string {
    return this.password.username ?
        this.i18n(
            'passwordDetailsCardEditButtonAriaLabel',
            this.getCredentialTypeString_(),
            htmlEscape(this.password.username)) :
        this.i18n(
            'passwordDetailsCardEditButtonNoUsernameAriaLabel',
            this.getCredentialTypeString_());
  }

  protected getAriaLabelForDeleteButton_(): string {
    return this.password.username ?
        this.i18n(
            'passwordDetailsCardDeleteButtonAriaLabel',
            this.getCredentialTypeString_(),
            htmlEscape(this.password.username)) :
        this.i18n(
            'passwordDetailsCardDeleteButtonNoUsernameAriaLabel',
            this.getCredentialTypeString_());
  }

  protected getAriaLabelForShareButton_(): string {
    return this.password.username ?
        this.i18n(
            'passwordDetailsCardShareButtonAriaLabel',
            this.getCredentialTypeString_(),
            htmlEscape(this.password.username)) :
        this.i18n(
            'passwordDetailsCardShareButtonNoUsernameAriaLabel',
            this.getCredentialTypeString_());
  }

  protected onMovePasswordClick_(e: Event) {
    assert(this.isAccountStoreUser);
    e.preventDefault();

    PasswordManagerImpl.getInstance().movePasswordsToAccount(
        [this.password.id]);
    this.fire(
        'passwords-moved',
        {accountEmail: this.accountEmail, numberOfPasswords: 1});
    recordMoveToAccountStoreAccepted(this.trigger_);
  }

  protected showMovePasswordEntry_(): boolean {
    return this.isUsingAccountStore &&
        this.password.storedIn ===
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
  }

  private maybeRegisterSharingHelpBubble_(): void {
    // Register the help bubble only if this is the first card in the list
    // (`shouldRegisterSharingPromo` is true), and the share button is visible
    // and not disabled.
    if (!this.shouldRegisterSharingPromo || !this.showShareButton_ ||
        this.passwordSharingDisabled_) {
      return;
    }

    this.registerHelpBubble(
        PASSWORD_SHARE_BUTTON_BUTTON_ELEMENT_ID, this.$.shareButtonContainer);
  }

  protected getCloudUploadIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:cloud-upload' :
        'passwords-icon:upload-old';
  }

  protected showCompromiseWarning_(): boolean {
    if (!loadTimeData.getBoolean('showCompromiseWarningInDetailsCard')) {
      return false;
    }
    const info = this.password.compromisedInfo;
    if (!info || info.isMuted) {
      return false;
    }
    const types = info.compromiseTypes;
    return types.includes(chrome.passwordsPrivate.CompromiseType.LEAKED) ||
        types.includes(chrome.passwordsPrivate.CompromiseType.PHISHED);
  }

  protected onChangePasswordClick_() {
    assert(this.password.changePasswordUrl);
    OpenWindowProxyImpl.getInstance().openUrl(this.password.changePasswordUrl);
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.CHANGE_PASSWORD_CLICKED);
  }

  protected onWarningDescClick_(e: Event) {
    const target = e.target as HTMLElement;
    if (target.tagName === 'A') {
      e.preventDefault();
      Router.getInstance().navigateTo(Page.CHECKUP);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-details-card': PasswordDetailsCardElement;
  }
}

customElements.define(
    PasswordDetailsCardElement.is, PasswordDetailsCardElement);
