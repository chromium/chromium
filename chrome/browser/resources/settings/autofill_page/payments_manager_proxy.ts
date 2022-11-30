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
  getCreditCardList(
      callback: (entries: chrome.autofillPrivate.CreditCardEntry[]) => void):
      void;

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
  setCreditCardFIDOAuthEnabledState(enabled: boolean): void;

  /**
   * Requests the list of UPI IDs from personal data.
   */
  getUpiIdList(callback: (entries: string[]) => void): void;

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

  getCreditCardList(
      callback: (entries: chrome.autofillPrivate.CreditCardEntry[]) => void) {
    chrome.autofillPrivate.getCreditCardList(callback);
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

  migrateCreditCards() {
    chrome.autofillPrivate.migrateCreditCards();
  }

  logServerCardLinkClicked() {
    chrome.autofillPrivate.logServerCardLinkClicked();
  }

  setCreditCardFIDOAuthEnabledState(enabled: boolean) {
    chrome.autofillPrivate.setCreditCardFIDOAuthEnabledState(enabled);
  }

  getUpiIdList(callback: (entries: string[]) => void) {
    chrome.autofillPrivate.getUpiIdList(callback);
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

  static getInstance(): PaymentsManagerProxy {
    return instance || (instance = new PaymentsManagerImpl());
  }

  static setInstance(obj: PaymentsManagerProxy) {
    instance = obj;
  }
}

let instance: PaymentsManagerProxy|null = null;
