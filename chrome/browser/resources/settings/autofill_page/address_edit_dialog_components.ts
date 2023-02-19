// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
export abstract class AddressComponentUi<ValueT> {
  protected readonly property: KeySubset<AddressEntry, ValueT>;

  private readonly address_: AddressEntry;
  private readonly originalAddress_?: AddressEntry;
  private readonly skipValidation_: boolean;
  private isValidatable_: boolean;
  readonly isTextarea: boolean;
  readonly isRequired: boolean;
  readonly label: string;
  readonly additionalClassName: string;

  constructor(
      address: AddressEntry,
      originalAddress: AddressEntry|undefined,
      label: string,
      additionalClassName: string = '',
      isTextarea: boolean = false,
      skipValidation: boolean = false,
      isRequired: boolean = false,
  ) {
    this.address_ = address;
    this.originalAddress_ = originalAddress;
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

  /**
   * Gets the value from the address that's associated with this component.
   */
  protected abstract getValue(address: AddressEntry): string|undefined;

  /**
   * Sets the value in the address that's associated with this component.
   */
  protected abstract setValue(value: string|undefined, address: AddressEntry):
      void;

  private hasValue_(address = this.address_): boolean {
    return isValueNonEmpty(this.getValue(address));
  }
}

/**
 * Base class for address fields whose value is stored as
 * a simple string (optional) property.
 */
export class StringComponentUi extends AddressComponentUi<string|undefined> {
  protected getValue(address: AddressEntry): string|undefined {
    return address[this.property];
  }

  protected setValue(value: string|undefined, address: AddressEntry): void {
    address[this.property] = value;
  }
}

/**
 * Base class for address fields whose value is stored as
 * a single valued string array (optional) property, for
 * historical reason, see crbug.com/497934 for details.
 */
export class ArrayStringComponentUi extends
    AddressComponentUi<string[]|undefined> {
  protected getValue(address: AddressEntry): string|undefined {
    const value = address[this.property];
    return value ? value[0] : undefined;
  }

  protected setValue(value: string|undefined, address: AddressEntry): void {
    address[this.property] = isValueNonEmpty(value) ? [value!] : [];
  }
}

export class HonorificComponentUi extends StringComponentUi {
  protected override readonly property = 'honorific';
}
export class CompanyNameComponentUi extends StringComponentUi {
  protected override readonly property = 'companyName';
}
export class FullNamesComponentUi extends ArrayStringComponentUi {
  protected override readonly property = 'fullNames';
}
export class AddressLinesComponentUi extends StringComponentUi {
  protected override readonly property = 'addressLines';
}
export class AddressLevel1ComponentUi extends StringComponentUi {
  protected override readonly property = 'addressLevel1';
}
export class AddressLevel2ComponentUi extends StringComponentUi {
  protected override readonly property = 'addressLevel2';
}
export class AddressLevel3ComponentUi extends StringComponentUi {
  protected override readonly property = 'addressLevel3';
}
export class PostalCodeComponentUi extends StringComponentUi {
  protected override readonly property = 'postalCode';
}
export class CountryCodeComponentUi extends StringComponentUi {
  protected override readonly property = 'countryCode';
}
export class SortingCodeComponentUi extends StringComponentUi {
  protected override readonly property = 'sortingCode';
}
export class PhoneComponentUi extends ArrayStringComponentUi {
  protected override readonly property = 'phoneNumbers';
}
export class EmailComponentUi extends ArrayStringComponentUi {
  protected override readonly property = 'emailAddresses';
}
