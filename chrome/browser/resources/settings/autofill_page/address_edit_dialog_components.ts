// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';

type AddressEntry = chrome.autofillPrivate.AddressEntry;

/**
 * Utility type for taking a subset of property keys, whose value type
 * meets certain criteria.
 */
type KeySubset<T, ValueT> = keyof {
  [key in keyof T as T[key] extends ValueT ? key : never]: T[key];
};

function isValueNonEmpty(value: string|undefined): boolean {
  return value !== undefined && value !== '';
}

/**
 * The base class for data behind an address component. It exposes the `value`
 * property, which is how interface controls (e.g. input) communicate with it.
 */
export class AddressComponentUi {
  private readonly address_: AddressEntry;
  private readonly originalAddress_?: AddressEntry;
  private readonly property_: KeySubset<AddressEntry, string|undefined>;
  private readonly skipValidation_: boolean;
  private isValidatable_: boolean;
  readonly isTextarea: boolean;
  readonly isRequired: boolean;
  readonly label: string;
  readonly additionalClassName: string;

  constructor(
      address: AddressEntry,
      originalAddress: AddressEntry|undefined,
      fieldType: chrome.autofillPrivate.ServerFieldType,
      label: string,
      additionalClassName: string = '',
      isTextarea: boolean = false,
      skipValidation: boolean = false,
      isRequired: boolean = false,
  ) {
    this.address_ = address;
    this.originalAddress_ = originalAddress;
    this.property_ = this.getProperty(fieldType);
    this.label = label;
    this.additionalClassName = additionalClassName;
    this.isTextarea = isTextarea;
    this.isRequired = isRequired;
    this.skipValidation_ = skipValidation;
    this.isValidatable_ = false;
  }

  /**
   * Notifications from data class (as alternative to change listeners
   * in DOM) are important to sync actual data updates with validation.
   */
  onValueUpdateListener?: () => void;

  /**
   * Being validatable for an address component means that its invalid state
   * is visible to the user. Having a component not validatable initially
   * (before any interactions with controls) allows less aggressive validation
   * experience for the user.
   */
  get isValidatable(): boolean {
    return this.isValidatable_;
  }

  get isValid(): boolean {
    if (this.skipValidation_) {
      return true;
    }
    if (this.originalAddress_) {
      // "dont make it worse" validation for existing addresses:
      // consider a field valid as long as it is equal to the original value,
      // wheather it is valid or not.
      if (this.getValue(this.originalAddress_) ===
              this.getValue(this.address_) ||
          (!this.hasValue_(this.originalAddress_) && !this.hasValue_())) {
        return true;
      }
    }
    return !this.isRequired || !!this.value;
  }

  get value(): string|undefined {
    return this.getValue(this.address_);
  }

  set value(value: string|undefined) {
    const changed = value !== this.value;
    this.setValue(value, this.address_);
    if (changed) {
      this.onValueUpdateListener?.();
    }
  }

  get hasValue(): boolean {
    return this.hasValue_();
  }

  makeValidatable(): void {
    this.isValidatable_ = true;
  }

  // TODO(crbug.com/1441904): remove this switch case in favour of field
  // mapping in AddressEntry.
  protected getProperty(fieldType: chrome.autofillPrivate.ServerFieldType):
      KeySubset<AddressEntry, string|undefined> {
    switch (fieldType) {
      case chrome.autofillPrivate.ServerFieldType.NAME_FULL:
        return 'fullName';
      case chrome.autofillPrivate.ServerFieldType.NAME_HONORIFIC_PREFIX:
        return 'honorific';
      case chrome.autofillPrivate.ServerFieldType.COMPANY_NAME:
        return 'companyName';
      case chrome.autofillPrivate.ServerFieldType.ADDRESS_HOME_STREET_ADDRESS:
        return 'addressLines';
      case chrome.autofillPrivate.ServerFieldType.ADDRESS_HOME_STATE:
        return 'addressLevel1';
      case chrome.autofillPrivate.ServerFieldType.ADDRESS_HOME_CITY:
        return 'addressLevel2';
      case chrome.autofillPrivate.ServerFieldType
          .ADDRESS_HOME_DEPENDENT_LOCALITY:
        return 'addressLevel3';
      case chrome.autofillPrivate.ServerFieldType.ADDRESS_HOME_ZIP:
        return 'postalCode';
      case chrome.autofillPrivate.ServerFieldType.ADDRESS_HOME_SORTING_CODE:
        return 'sortingCode';
      case chrome.autofillPrivate.ServerFieldType.ADDRESS_HOME_COUNTRY:
        return 'countryCode';
      case chrome.autofillPrivate.ServerFieldType.PHONE_HOME_WHOLE_NUMBER:
        return 'phoneNumber';
      case chrome.autofillPrivate.ServerFieldType.EMAIL_ADDRESS:
        return 'emailAddress';
    }
    assertNotReached('Unsupported field type: ' + fieldType);
  }

  /**
   * Gets the value from the address that's associated with this component.
   */
  protected getValue(address: AddressEntry): string|undefined {
    return address[this.property_];
  }

  /**
   * Sets the value in the address that's associated with this component.
   */
  protected setValue(value: string|undefined, address: AddressEntry): void {
    address[this.property_] = value;
  }

  private hasValue_(address = this.address_): boolean {
    return isValueNonEmpty(this.getValue(address));
  }
}
