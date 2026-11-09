// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import './site_favicon.js';
import './searchable_label.js';
import './dialogs/move_passwords_dialog.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {htmlEscape} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './password_list_item.css.js';
import {getHtml} from './password_list_item.html.js';
import {PasswordManagerImpl, PasswordViewPageInteractions} from './password_manager_proxy.js';
import {Page, Router, UrlParam} from './router.js';

export interface PasswordListItemElement {
  $: {
    displayedName: HTMLElement,
    numberOfAccounts: HTMLElement,
    seePasswordDetails: HTMLElement,
  };
}
const PasswordListItemElementBase = I18nMixinLit(CrLitElement);

export class PasswordListItemElement extends PasswordListItemElementBase {
  static get is() {
    return 'password-list-item';
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
      isAccountStoreUser: {type: Boolean},
      first: {type: Boolean},
      searchTerm: {type: String},
      showMovePasswordDialog_: {type: Boolean},

      /**
       * The number of accounts in a group as a formatted string.
       */
      numberOfAccounts_: {type: String},

      tooltipText_: {type: String},
      deviceOnlyCredentialsAccessibilityLabelText_: {type: String},
    };
  }

  accessor item: chrome.passwordsPrivate.CredentialGroup;
  accessor isAccountStoreUser: boolean = false;
  accessor first: boolean = false;
  accessor searchTerm: string = '';
  protected accessor numberOfAccounts_: string = '';
  protected accessor tooltipText_: string = '';
  private accessor deviceOnlyCredentialsAccessibilityLabelText_: string = '';
  protected accessor showMovePasswordDialog_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('item') && this.item.entries.length > 1) {
      PluralStringProxyImpl.getInstance()
          .getPluralString('numberOfAccounts', this.item.entries.length)
          .then(count => {
            this.numberOfAccounts_ = count;
          });
    }
    if (changedProperties.has('item') ||
        changedProperties.has('isAccountStoreUser')) {
      this.onItemChanged_();
    }
  }

  override firstUpdated() {
    this.addEventListener('click', this.onRowClick_);
  }

  override focus() {
    this.$.seePasswordDetails.focus();
  }

  protected getElementClass_(): string {
    return this.first ? '' : 'hr';
  }

  private onRowClick_() {
    if (this.showMovePasswordDialog_) {
      return;
    }

    const ids = this.item.entries.map(entry => entry.id);
    PasswordManagerImpl.getInstance()
        .requestCredentialsDetails(ids)
        .then(entries => {
          const group: chrome.passwordsPrivate.CredentialGroup = {
            name: this.item.name,
            iconUrl: this.item.iconUrl,
            entries: entries,
          };
          this.fire('password-details-shown', this);
          // Keep current search query.
          Router.getInstance().navigateTo(
              Page.PASSWORD_DETAILS, group,
              Router.getInstance().currentRoute.queryParameters);
        })
        .catch(() => {});
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.CREDENTIAL_ROW_CLICKED);

    const searchTerm = Router.getInstance().currentRoute.queryParameters.get(
                           UrlParam.SEARCH_TERM) || '';
    chrome.metricsPrivate.recordBoolean(
        'PasswordManager.UI.OpenedPasswordDetailsWhileSearching', !!searchTerm);
  }

  private async onItemChanged_() {
    if (this.hasUploadablePasswords_()) {
      this.deviceOnlyCredentialsAccessibilityLabelText_ =
          await PluralStringProxyImpl.getInstance()
              .getPluralString(
                  'deviceOnlyListItemAriaLabel', this.item.entries.length)
              .then(label => label.replace('$1', this.item.name));

      this.tooltipText_ =
          await PluralStringProxyImpl.getInstance()
              .getPluralString(
                  'movePasswordToAccountIconTooltip',
                  this.getCredentialsOnDevice_().length)
              .then((label: string) => label.replace('$1', this.item.name));
    } else {
      this.showMovePasswordDialog_ = false;
    }
  }

  protected showNumberOfAccounts_(): boolean {
    return !this.searchTerm && this.item.entries.length > 1;
  }

  protected getTitle_() {
    const term = this.searchTerm.trim().toLowerCase();
    if (!term) {
      return this.item.name;
    }

    if (this.item.name.includes(term)) {
      return this.item.name;
    }

    const entries = this.item.entries;
    // Show matching username in the title if it exists.
    const matchingUsername =
        entries.find(c => c.username.toLowerCase().includes(term))?.username;
    if (matchingUsername) {
      return this.item.name + ' • ' + matchingUsername;
    }

    // Show matching domain in the title if it exists.
    const domains =
        Array.prototype.concat(...entries.map(c => c.affiliatedDomains || []));
    const matchingDomain =
        domains.find(d => d.name.toLowerCase().includes(term))?.name;
    if (matchingDomain) {
      return this.item.name + ' • ' + matchingDomain;
    }
    return this.item.name;
  }

  protected getCredentialsOnDevice_():
      chrome.passwordsPrivate.PasswordUiEntry[] {
    return this.item.entries.filter(
        entry =>
            entry.storedIn === chrome.passwordsPrivate.PasswordStoreSet.DEVICE);
  }

  protected hasOnlyOneDeviceCredential_(): boolean {
    return this.getCredentialsOnDevice_().length === this.item.entries.length &&
        this.getCredentialsOnDevice_().length === 1;
  }

  protected hasUploadablePasswords_(): boolean {
    return this.isAccountStoreUser &&
        (this.getCredentialsOnDevice_().length > 0);
  }

  protected onMovePasswordDialogClose_(): void {
    this.showMovePasswordDialog_ = false;
  }

  protected onUploadButtonClick_(): void {
    this.showMovePasswordDialog_ = true;
  }

  protected getAriaLabel_(): string {
    if (this.hasUploadablePasswords_()) {
      return this.deviceOnlyCredentialsAccessibilityLabelText_;
    }
    return this.i18n('viewPasswordAriaDescription', htmlEscape(this.item.name));
  }

  protected getCloudUploadIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:cloud-upload' :
        'passwords-icon:password-cloud-upload-old';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-list-item': PasswordListItemElement;
  }
}

customElements.define(PasswordListItemElement.is, PasswordListItemElement);
