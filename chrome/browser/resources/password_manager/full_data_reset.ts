// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './shared_style.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './full_data_reset.html.js';
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

const FullDataResetElementBase = I18nMixin(PolymerElement);

export class FullDataResetElement extends FullDataResetElementBase {
  static get is() {
    return 'full-data-reset';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isSyncingPasswords: {
        type: Boolean,
        value: false,
      },

      isAccountStoreUser: {
        type: Boolean,
        value: false,
      },

      passwordsCount_: {
        type: String,
        value: '',
      },

      passkeysCount_: {
        type: String,
        value: '',
      },

      passwordsCountDetails_: {
        type: String,
        value: '',
      },

      passkeysCountDetails_: {
        type: String,
        value: '',
      },
    };
  }

  isSyncingPasswords: boolean;
  isAccountStoreUser: boolean;
  private passwordsCount_: string = '';
  private passkeysCount_: string = '';
  private passwordsCountDetails_: string = '';
  private passkeysCountDetails_: string = '';

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

  private onDeleteAllClick_(): void {
    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        credentials => this.updateCounters_(credentials));
    this.$.dialog.showModal();
  }

  private onCancel_(): void {
    this.$.dialog.close();
  }

  private async onConfirm_() {
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

  private getConfirmationDialogTitle_(): string {
    if (this.isSyncingPasswords || this.isAccountStoreUser) {
      return this.i18n('fullResetConfirmationTitle');
    }
    return this.i18n('fullResetConfirmationTitleLocal');
  }

  private getAriaLabel_(): string {
    return [
      this.i18n('fullResetTitle'),
      this.i18n('fullResetRowDescription'),
    ].join('. ');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'full-data-reset': FullDataResetElement;
  }
}

customElements.define(FullDataResetElement.is, FullDataResetElement);
