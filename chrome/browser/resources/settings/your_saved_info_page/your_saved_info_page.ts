// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-your-saved-info-page' is the entry point for users to see
 * and manage their saved info.
 */

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {AutofillManagerProxy, PersonalDataChangedListener} from '../autofill_page/autofill_manager_proxy.js';
import {AutofillManagerImpl} from '../autofill_page/autofill_manager_proxy.js';
import {PasswordManagerImpl, PasswordManagerPage} from '../autofill_page/password_manager_proxy.js';
import {PaymentsManagerImpl} from '../autofill_page/payments_manager_proxy.js';
import type {PaymentsManagerProxy} from '../autofill_page/payments_manager_proxy.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {SavedInfoHandlerImpl} from './saved_info_handler_proxy.js';
import {getTemplate} from './your_saved_info_page.html.js';


const SettingsYourSavedInfoPageElementBase =
    WebUiListenerMixin(SettingsViewMixin(PrefsMixin(PolymerElement)));

export class SettingsYourSavedInfoPageElement extends
    SettingsYourSavedInfoPageElementBase {
  static get is() {
    return 'settings-your-saved-info-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      passwordsCount: Number,
      passkeysCount: Number,
      addressesCount: Number,
      creditCardsCount: Number,
      ibansCount: Number,
      payOverTimeIssuersCount: Number,
    };
  }

  declare prefs: {[key: string]: any};
  declare passwordsCount: number|undefined;
  declare passkeysCount: number|undefined;
  declare addressesCount: number|undefined;
  declare creditCardsCount: number|undefined;
  declare ibansCount: number|undefined;
  declare payOverTimeIssuersCount: number|undefined;

  private paymentsManager_: PaymentsManagerProxy =
      PaymentsManagerImpl.getInstance();
  private autofillManager_: AutofillManagerProxy =
      AutofillManagerImpl.getInstance();
  private setPersonalDataListener_: PersonalDataChangedListener|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.setupDataTypeCounters();
  }

  private setupDataTypeCounters() {
    // Password and passkey counts.
    const setPasswordCount =
      (count: { passwordCount: number, passkeyCount: number }) => {
        this.passwordsCount = count.passwordCount;
        this.passkeysCount = count.passkeyCount;
      };
    this.addWebUiListener('password-count-changed', setPasswordCount);
    SavedInfoHandlerImpl.getInstance().getPasswordCount().then(
      setPasswordCount);

    // Addresses: Request initial data.
    const setAddressesListener =
      (addresses: chrome.autofillPrivate.AddressEntry[]) => {
        this.addressesCount = addresses.length;
      };
    this.autofillManager_.getAddressList().then(setAddressesListener);

    // Payments: Request initial data.
    const setCreditCardsListener =
      (creditCards: chrome.autofillPrivate.CreditCardEntry[]) => {
      this.creditCardsCount = creditCards.length;
    };
    const setIbansListener = (ibans: chrome.autofillPrivate.IbanEntry[]) => {
      this.ibansCount = ibans.length;
    };
    const setPayOverTimeListener =
      (payOverTimeIssuers: chrome.autofillPrivate.PayOverTimeIssuerEntry[]) => {
      this.payOverTimeIssuersCount = payOverTimeIssuers.length;
    };
    this.paymentsManager_.getCreditCardList().then(setCreditCardsListener);
    this.paymentsManager_.getIbanList().then(setIbansListener);
    this.paymentsManager_.getPayOverTimeIssuerList().then(
      setPayOverTimeListener);

    // Addresses and Payments: Listen for changes.
    const setPersonalDataListener: PersonalDataChangedListener =
      (addresses: chrome.autofillPrivate.AddressEntry[],
        creditCards: chrome.autofillPrivate.CreditCardEntry[],
        ibans: chrome.autofillPrivate.IbanEntry[],
        payOverTimeIssuers: chrome.autofillPrivate.PayOverTimeIssuerEntry[],
        _accountInfo?: chrome.autofillPrivate.AccountInfo) => {
        this.addressesCount = addresses.length;
        this.creditCardsCount = creditCards.length;
        this.ibansCount = ibans.length;
        this.payOverTimeIssuersCount = payOverTimeIssuers.length;
      };
    this.setPersonalDataListener_ = setPersonalDataListener;
    this.autofillManager_.setPersonalDataManagerListener(
      setPersonalDataListener);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.setPersonalDataListener_) {
      this.autofillManager_.removePersonalDataManagerListener(
          this.setPersonalDataListener_);
      this.setPersonalDataListener_ = null;
    }
  }

  /**
   * Shows Password Manager page.
   */
  private onPasswordManagerExternalLinkClick() {
    PasswordManagerImpl.getInstance().recordPasswordsPageAccessInSettings();
    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.PASSWORDS);
  }

  /**
   * Opens Wallet page in a new tab.
   */
  private onGoogleWalletExternalLinkClick() {
    window.open('https://wallet.google.com');
  }

  /**
   * Opens Google Account page in a new tab.
   */
  private onGoogleAccountExternalLinkClick() {
    window.open('https://myaccount.google.com');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-your-saved-info-page': SettingsYourSavedInfoPageElement;
  }
}

customElements.define(
    SettingsYourSavedInfoPageElement.is, SettingsYourSavedInfoPageElement);
