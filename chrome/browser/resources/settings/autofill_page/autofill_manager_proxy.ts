// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type PersonalDataChangedListener =
    (addresses: chrome.autofillPrivate.AddressEntry[],
     creditCards: chrome.autofillPrivate.CreditCardEntry[],
     ibans: chrome.autofillPrivate.IbanEntry[],
     accountInfo?: chrome.autofillPrivate.AccountInfo) => void;

/**
 * Interface for all callbacks to the autofill API.
 */
export interface AutofillManagerProxy {
  /**
   * Gets currently signed-in user account info, or undefined if not signed-in.
   */
  getAccountInfo(): Promise<chrome.autofillPrivate.AccountInfo|undefined>;

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
   * Request the list of addresses.
   */
  getAddressList(): Promise<chrome.autofillPrivate.AddressEntry[]>;

  /**
   * Saves the given address.
   */
  saveAddress(address: chrome.autofillPrivate.AddressEntry): void;

  /** @param guid The guid of the address to remove.  */
  removeAddress(guid: string): void;

  /**
   * Sets the Sync Autofill toggle value.
   */
  setAutofillSyncToggleEnabled(enabled: boolean): void;
}

/**
 * Implementation that accesses the private API.
 */
export class AutofillManagerImpl implements AutofillManagerProxy {
  getAccountInfo(): Promise<chrome.autofillPrivate.AccountInfo|undefined> {
    return chrome.autofillPrivate.getAccountInfo();
  }

  setPersonalDataManagerListener(listener: PersonalDataChangedListener) {
    chrome.autofillPrivate.onPersonalDataChanged.addListener(listener);
  }

  removePersonalDataManagerListener(listener: PersonalDataChangedListener) {
    chrome.autofillPrivate.onPersonalDataChanged.removeListener(listener);
  }

  getAddressList() {
    return chrome.autofillPrivate.getAddressList();
  }

  saveAddress(address: chrome.autofillPrivate.AddressEntry) {
    chrome.autofillPrivate.saveAddress(address);
  }

  removeAddress(guid: string) {
    chrome.autofillPrivate.removeEntry(guid);
  }

  setAutofillSyncToggleEnabled(enabled: boolean) {
    chrome.autofillPrivate.setAutofillSyncToggleEnabled(enabled);
  }

  static getInstance(): AutofillManagerProxy {
    return instance || (instance = new AutofillManagerImpl());
  }

  static setInstance(obj: AutofillManagerProxy) {
    instance = obj;
  }
}

let instance: AutofillManagerProxy|null = null;
