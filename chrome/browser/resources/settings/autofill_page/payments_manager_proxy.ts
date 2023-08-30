// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PersonalDataChangedListener} from './autofill_manager_proxy.js';

/**
 * Interface for all callbacks to the payments autofill API.
 */
export interface PaymentsManagerProxy {
  /**
   * Add an observer to the list of personal data.
   */
  setPersonalDataManagerListener(listener: PersonalDataChangedListener): void;

  /**
   * Remove an observer from the list of personal data.
   */
  removePersonalDataManagerListener(listener: PersonalDataChangedListener):
      void;

  /**
   * Request the list of credit cards.
   */
  getCreditCardList(): Promise<chrome.autofillPrivate.CreditCardEntry[]>;

  /**
   * Request the list of IBANs.
   */
  getIbanList(): Promise<chrome.autofillPrivate.IbanEntry[]>;

  /** @param ibanValue Returns true if the given ibanValue is valid. */
  isValidIban(ibanValue: string): Promise<boolean>;

  /** @param guid The GUID of the credit card to remove. */
  removeCreditCard(guid: string): void;

  /**
   * @param guid The GUID to credit card to remove from the cache.
   */
  clearCachedCreditCard(guid: string): void;

  /**
   * Saves the given credit card.
   */
  saveCreditCard(creditCard: chrome.autofillPrivate.CreditCardEntry): void;

  /** @param guid The GUID of the IBAN to remove. */
  removeIban(guid: string): void;

  /**
   * Saves the given IBAN.
   */
  saveIban(iban: chrome.autofillPrivate.IbanEntry): void;

  /**
   * Migrate the local credit cards.
   */
  migrateCreditCards(): void;

  /**
   * Logs that the server cards edit link was clicked.
   */
  logServerCardLinkClicked(): void;

  /**
   * Enables FIDO authentication for card unmasking.
   */
  setCreditCardFidoAuthEnabledState(enabled: boolean): void;

  /**
   * Enrolls the card into virtual cards.
   */
  addVirtualCard(cardId: string): void;

  /**
   * Unenrolls the card from virtual cards.
   */
  removeVirtualCard(cardId: string): void;

  /**
   * A null response means that there is no platform authenticator.
   */
  isUserVerifyingPlatformAuthenticatorAvailable(): Promise<boolean|null>;

  /**
   * Authenticate the user via device authentication and flip the mandatory auth
   * toggle is successful.
   */
  authenticateUserAndFlipMandatoryAuthToggle(): void;

  /**
   * Authenticate the user via device authentication and display the edit dialog
   * for local card if the auth is successful.
   */
  authenticateUserToEditLocalCard(): Promise<boolean>;

  // <if expr="is_win or is_macosx">
  /**
   * Returns true if there is authentication available on this device (biometric
   * or screen lock), false otherwise.
   */
  checkIfDeviceAuthAvailable(): Promise<boolean>;
  // </if>
}

/**
 * Implementation that accesses the private API.
 */
export class PaymentsManagerImpl implements PaymentsManagerProxy {
  setPersonalDataManagerListener(listener: PersonalDataChangedListener) {
    chrome.autofillPrivate.onPersonalDataChanged.addListener(listener);
  }

  removePersonalDataManagerListener(listener: PersonalDataChangedListener) {
    chrome.autofillPrivate.onPersonalDataChanged.removeListener(listener);
  }

  getCreditCardList() {
    return chrome.autofillPrivate.getCreditCardList();
  }

  getIbanList() {
    return chrome.autofillPrivate.getIbanList();
  }

  isValidIban(ibanValue: string) {
    return chrome.autofillPrivate.isValidIban(ibanValue);
  }

  removeCreditCard(guid: string) {
    chrome.autofillPrivate.removeEntry(guid);
  }

  clearCachedCreditCard(guid: string) {
    chrome.autofillPrivate.maskCreditCard(guid);
  }

  saveCreditCard(creditCard: chrome.autofillPrivate.CreditCardEntry) {
    chrome.autofillPrivate.saveCreditCard(creditCard);
  }

  saveIban(iban: chrome.autofillPrivate.IbanEntry) {
    chrome.autofillPrivate.saveIban(iban);
  }

  removeIban(guid: string) {
    chrome.autofillPrivate.removeEntry(guid);
  }

  migrateCreditCards() {
    chrome.autofillPrivate.migrateCreditCards();
  }

  logServerCardLinkClicked() {
    chrome.autofillPrivate.logServerCardLinkClicked();
  }

  setCreditCardFidoAuthEnabledState(enabled: boolean) {
    chrome.autofillPrivate.setCreditCardFIDOAuthEnabledState(enabled);
  }

  addVirtualCard(cardId: string) {
    chrome.autofillPrivate.addVirtualCard(cardId);
  }

  removeVirtualCard(serverId: string) {
    chrome.autofillPrivate.removeVirtualCard(serverId);
  }

  isUserVerifyingPlatformAuthenticatorAvailable() {
    if (!window.PublicKeyCredential) {
      return Promise.resolve(null);
    }

    return window.PublicKeyCredential
        .isUserVerifyingPlatformAuthenticatorAvailable();
  }

  authenticateUserAndFlipMandatoryAuthToggle() {
    chrome.autofillPrivate.authenticateUserAndFlipMandatoryAuthToggle();
  }

  authenticateUserToEditLocalCard() {
    return chrome.autofillPrivate.authenticateUserToEditLocalCard();
  }

  // <if expr="is_win or is_macosx">
  checkIfDeviceAuthAvailable() {
    return chrome.autofillPrivate.checkIfDeviceAuthAvailable();
  }
  // </if>

  static getInstance(): PaymentsManagerProxy {
    return instance || (instance = new PaymentsManagerImpl());
  }

  static setInstance(obj: PaymentsManagerProxy) {
    instance = obj;
  }
}

let instance: PaymentsManagerProxy|null = null;
