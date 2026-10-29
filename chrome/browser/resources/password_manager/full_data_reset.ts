// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './full_data_reset.css.js';
import {getHtml} from './full_data_reset.html.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

export interface FullDataResetElement {
  $: {
    confirmationDialogTitle: HTMLElement,
    deleteAllButton: CrButtonElement,
    cancelButton: CrButtonElement,
    confirmButton: CrButtonElement,
    dialog: CrDialogElement,
    successToast: CrToastElement,
  };
}

const FullDataResetElementBase = I18nMixinLit(CrLitElement);

export class FullDataResetElement extends FullDataResetElementBase {
  static get is() {
    return 'full-data-reset';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isSyncingPasswords: {type: Boolean},
      isAccountStoreUser: {type: Boolean},
      passwordsCount_: {type: String},
      passkeysCount_: {type: String},
      passwordsCountDetails_: {type: String},
      passkeysCountDetails_: {type: String},
    };
  }

  accessor isSyncingPasswords: boolean = false;
  accessor isAccountStoreUser: boolean = false;
  protected accessor passwordsCount_: string = '';
  protected accessor passkeysCount_: string = '';
  protected accessor passwordsCountDetails_: string = '';
  protected accessor passkeysCountDetails_: string = '';

  private async updateCounters_(credentials:
                                    chrome.passwordsPrivate.PasswordUiEntry[]) {
    let numPasswords = 0;
    let numPasskeys = 0;
    const passwordSites = new Set<string>();
    const passkeySites = new Set<string>();

    for (const credential of credentials) {
      if (credential.isPasskey) {
        numPasskeys++;
        credential.affiliatedDomains.forEach(
            domain => passkeySites.add(domain.name));
      } else {
        numPasswords++;
        credential.affiliatedDomains.forEach(
            domain => passwordSites.add(domain.name));
      }
    }

    const [
        passwordCountString,
        passkeysCountString,
        passwordDetails,
        passkeysDetails,
      ] = await Promise.all([
      this.getFormattedCountString('fullResetPasswordsCounter', numPasswords),
      this.getFormattedCountString('fullResetPasskeysCounter', numPasskeys),
      this.getFormattedSiteDetails(passwordSites),
      this.getFormattedSiteDetails(passkeySites),
    ]);

    this.passwordsCount_ = passwordCountString;
    this.passkeysCount_ = passkeysCountString;
    this.passwordsCountDetails_ = passwordDetails;
    this.passkeysCountDetails_ = passkeysDetails;
  }

  private async getFormattedCountString(key: string, count: number):
      Promise<string> {
    return PluralStringProxyImpl.getInstance().getPluralString(key, count);
  }

  private async getFormattedSiteDetails(sites: Set<string>): Promise<string> {
    const sitesArray = [...sites];

    switch (sites.size) {
      case 0:
        return '';
      case 1:
        return this.i18n('fullResetDomainsDisplayOne', sitesArray[0]);
      case 2:
        return this.i18n(
            'fullResetDomainsDisplayTwo', sitesArray[0], sitesArray[1]);
      default:
        const moreCount = sites.size - 2;
        const container = await this.getFormattedCountString(
            'fullResetDomainsDisplayTwoAndXMore', moreCount);
        return container.replace('$1', sitesArray[0])
            .replace('$2', sitesArray[1]);
    }
  }

  protected onDeleteAllClick_(): void {
    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        credentials => this.updateCounters_(credentials));
    this.$.dialog.showModal();
  }

  protected onCancelClick_(): void {
    this.$.dialog.close();
  }

  protected async onConfirmClick_() {
    this.$.dialog.close();
    const success =
        await PasswordManagerImpl.getInstance().deleteAllPasswordManagerData();
    this.showToastWithResult_(success);
  }

  private showToastWithResult_(success: boolean) {
    if (success) {
      this.$.successToast.show();
    } else {
      // TODO(crbug.com/342366264): Show error toast.
    }
  }

  protected getConfirmationDialogTitle_(): string {
    if (this.isSyncingPasswords || this.isAccountStoreUser) {
      return this.i18n('fullResetConfirmationTitle');
    }
    return this.i18n('fullResetConfirmationTitleLocal');
  }

  protected getAriaDescription_(): string {
    return [
      this.i18n('fullResetTitle'),
      this.i18n('fullResetRowDescription'),
    ].join('. ');
  }

  protected getWarningIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:warning' :
        'passwords-icon:outlined-warning-old';
  }

  protected getPasskeyIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:passkey' :
        'passwords-icon:passkey-filled-old';
  }

  protected getPasswordIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:password' :
        'passwords-icon:password-old';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'full-data-reset': FullDataResetElement;
  }
}

customElements.define(FullDataResetElement.is, FullDataResetElement);
