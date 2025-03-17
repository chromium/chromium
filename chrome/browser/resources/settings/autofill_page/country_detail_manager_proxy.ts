// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type AddressComponents = chrome.autofillPrivate.AddressComponents;
type CountryEntry = chrome.autofillPrivate.CountryEntry;

/**
 * This interface defines the autofill API wrapper that combines country related
 * methods.
 */
export interface CountryDetailManagerProxy {
  /**
   * Gets the list of available countries.
   * The default country will be first, followed by a separator, followed by
   * an alphabetized list of countries available.
   */
  getCountryList(forAccountStorage: boolean): Promise<CountryEntry[]>;

  /**
   * Gets the address format for a given country code.
   */
  getAddressFormat(countryCode: string): Promise<AddressComponents>;
}

export class CountryDetailManagerProxyImpl implements
    CountryDetailManagerProxy {
  getCountryList(forAccountStorage: boolean) {
    return chrome.autofillPrivate.getCountryList(forAccountStorage);
  }

  getAddressFormat(countryCode: string) {
    return chrome.autofillPrivate.getAddressComponents(countryCode);
  }

  static getInstance() {
    return instance || (instance = new CountryDetailManagerProxyImpl());
  }

  static setInstance(obj: CountryDetailManagerProxy) {
    instance = obj;
  }
}

let instance: CountryDetailManagerProxy|null = null;
