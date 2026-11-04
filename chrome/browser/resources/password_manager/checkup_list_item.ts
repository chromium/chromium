// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './dialogs/edit_password_dialog.js';
import './dialogs/delete_password_disclaimer_dialog.js';
import './dialogs/edit_password_disclaimer_dialog.js';
import './site_favicon.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './checkup_list_item.css.js';
import {getHtml} from './checkup_list_item.html.js';
import {PasswordAutomaticChangeState} from './password_manager.mojom-webui.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import type {ShowPasswordMixinLitInterface} from './show_password_mixin_lit.js';
import {ShowPasswordMixinLit} from './show_password_mixin_lit.js';

export interface CheckupListItemElement extends ShowPasswordMixinLitInterface {
  $: {
    shownUrl: HTMLElement,
    username: HTMLElement,
    insecurePassword: HTMLInputElement,
    more: CrIconButtonElement,
  };
}

const CheckupListItemElementBase =
    ShowPasswordMixinLit(I18nMixinLit(CrLitElement));

export class CheckupListItemElement extends CheckupListItemElementBase {
  static get is() {
    return 'checkup-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      item: {type: Object},
      group: {type: Object},
      first: {type: Boolean},
      showDetails: {type: Boolean},
      showAlreadyChanged: {type: Boolean},
      showEditPasswordDialog_: {type: Boolean},
      showEditPasswordDisclaimer_: {type: Boolean},
      showDeletePasswordDialog_: {type: Boolean},
      passwordChangeState: {type: Number},
      isCancelDisabled_: {type: Boolean},
    };
  }

  accessor item: chrome.passwordsPrivate.PasswordUiEntry;
  accessor group: chrome.passwordsPrivate.CredentialGroup|undefined;
  accessor first: boolean = false;
  accessor showDetails: boolean = false;
  accessor showAlreadyChanged: boolean = false;
  accessor passwordChangeState: PasswordAutomaticChangeState =
      PasswordAutomaticChangeState.kInactive;
  protected accessor isCancelDisabled_: boolean = false;
  protected accessor showEditPasswordDialog_: boolean = false;
  protected accessor showEditPasswordDisclaimer_: boolean = false;
  protected accessor showDeletePasswordDialog_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('passwordChangeState')) {
      this.isCancelDisabled_ = false;
    }
  }

  protected getPasswordValue_(): string {
    return (this.isPasswordVisible && this.item.password) ? this.item.password :
                                                            ' '.repeat(10);
  }

  protected getCompromiseDescription_(): string {
    assert(this.item.compromisedInfo);
    const isLeaked = this.item.compromisedInfo.compromiseTypes.some(
        type => type === chrome.passwordsPrivate.CompromiseType.LEAKED);
    const isPhished = this.item.compromisedInfo.compromiseTypes.some(
        type => type === chrome.passwordsPrivate.CompromiseType.PHISHED);
    if (isLeaked && isPhished) {
      return this.i18n('phishedAndLeakedPassword');
    }
    if (isPhished) {
      return this.i18n('phishedPassword');
    }
    if (isLeaked) {
      return this.i18n('leakedPassword');
    }

    assertNotReached(
        'Can\'t find a string for type: ' + this.item.compromisedInfo);
  }

  protected onMoreClick_(event: Event) {
    this.fire('more-actions-click', {
      listItem: this,
      target: event.target,
    });
  }

  showHidePassword() {
    if (this.isPasswordVisible === true) {
      this.onShowHidePasswordButtonClick();
      this.item.password = undefined;
      this.item.note = undefined;
      return;
    }

    PasswordManagerImpl.getInstance()
        .requestCredentialsDetails([this.item.id])
        .then(entries => {
          const entry = entries[0];
          assert(!!entry);
          this.item.password = entry.password;
          this.item.note = entry.note;
          this.onShowHidePasswordButtonClick();
        })
        .catch(() => {});
  }

  showEditDialog() {
    PasswordManagerImpl.getInstance()
        .requestCredentialsDetails([this.item.id])
        .then(entries => {
          const entry = entries[0];
          assert(!!entry);
          this.item.affiliatedDomains = entry.affiliatedDomains;
          this.item.password = entry.password;
          this.item.note = entry.note;
          this.showEditPasswordDialog_ = true;
        })
        .catch(() => {});
  }

  showDeleteDialog() {
    this.showDeletePasswordDialog_ = true;
  }

  protected onChangePasswordClick_() {
    assert(this.item.changePasswordUrl);
    OpenWindowProxyImpl.getInstance().openUrl(this.item.changePasswordUrl);
    this.fire('change-password-clicked', this.item.id);
  }

  protected onAutoChangePasswordClick_() {
    if (this.item && this.item.isAutomaticPasswordChangeSupported) {
      PasswordManagerImpl.getInstance().requestChangePassword(this.item.id);
    }
  }

  protected onOpenInNewClick_() {
    PasswordManagerImpl.getInstance().openPasswordChangeTab(this.item.id);
  }

  protected onCancelAutoChangeClick_() {
    this.isCancelDisabled_ = true;
    PasswordManagerImpl.getInstance().stopPasswordChange(this.item.id);
  }

  protected onAlreadyChangedClick_(e: Event) {
    this.showEditPasswordDisclaimer_ = true;
    e.preventDefault();
  }

  protected onEditPasswordDialogClose_() {
    this.showEditPasswordDialog_ = false;
    this.item.password = undefined;
    this.item.note = undefined;
  }

  protected onEditPasswordClick_() {
    this.showEditDialog();
  }

  protected onEditDisclaimerClose_() {
    this.showEditPasswordDisclaimer_ = false;
  }

  protected onDeletePasswordDialogClose_() {
    this.showDeletePasswordDialog_ = false;
  }

  protected onDeletePasswordClick_() {
    PasswordManagerImpl.getInstance().removeCredential(
        this.item.id, this.item.storedIn);
    this.fire('password-removed', {
      removedFromStores: this.item.storedIn,
    });
  }

  protected getGroupName_(): string {
    return !this.group ? '' : this.group.name;
  }

  protected getGroupIcon_(): string {
    return !this.group ? '' : this.group.iconUrl;
  }

  protected getChangeButtonAriaLabel_(): string {
    return this.i18n('changePasswordAriaDescription', this.getGroupName_());
  }

  protected getAutoChangeButtonAriaLabel_(): string {
    return this.i18n(
        'automatedPasswordChangeCheckupButtonAriaDescription',
        this.getGroupName_());
  }

  protected getMoreButtonAriaLabel_(): string {
    return this.i18n('moreActionsAriaDescription', this.getGroupName_());
  }

  protected getOpenInNewButtonAriaLabel_(): string {
    return this.i18n('changePasswordAriaDescription', this.getGroupName_());
  }

  protected isAutoChangePasswordIdle_(): boolean {
    return this.passwordChangeState === PasswordAutomaticChangeState.kInactive;
  }

  protected hideCancelButton_(): boolean {
    return this.passwordChangeState !==
        PasswordAutomaticChangeState.kAttemptingSignIn &&
        this.passwordChangeState !==
        PasswordAutomaticChangeState.kChangingPassword &&
        this.passwordChangeState !==
        PasswordAutomaticChangeState.kConfirmingChangedPassword;
  }

  protected shouldShowOpenInNew_(): boolean {
    return this.passwordChangeState !==
        PasswordAutomaticChangeState.kInactive &&
        this.passwordChangeState !==
        PasswordAutomaticChangeState.kPasswordChangedSuccessfully;
  }

  protected getAutoChangePasswordIcon_(): string {
    return this.passwordChangeState ===
            PasswordAutomaticChangeState.kPasswordChangedSuccessfully ?
        'passwords-icon:task-spark' :
        'passwords-icon:arrow-selector-spark';
  }

  protected getAutoChangePasswordButtonText_(): string {
    switch (this.passwordChangeState) {
      case PasswordAutomaticChangeState.kInactive:
        return this.i18n('automatedPasswordChangeCheckupButton');
      case PasswordAutomaticChangeState.kAttemptingSignIn:
        return this.i18n('automatedPasswordChangeAttemptingSignIn');
      case PasswordAutomaticChangeState.kChangingPassword:
        return this.i18n('automatedPasswordChangeChangingPassword');
      case PasswordAutomaticChangeState.kConfirmingChangedPassword:
        return this.i18n('automatedPasswordChangeConfirmingChangedPassword');
      case PasswordAutomaticChangeState.kPasswordChangedSuccessfully:
        return this.i18n('automatedPasswordChangeChangedSuccessfully');
      default:
        return this.i18n('automatedPasswordChangeError');
    }
  }

  protected getArrowSelectorSparkIcon_(): string {
    return 'passwords-icon:arrow-selector-spark';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'checkup-list-item': CheckupListItemElement;
  }
}

customElements.define(CheckupListItemElement.is, CheckupListItemElement);
