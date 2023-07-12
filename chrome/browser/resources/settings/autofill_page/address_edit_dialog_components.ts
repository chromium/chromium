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
export class AddressComponentUi {
  protected readonly property: KeySubset<AddressEntry, string|undefined>;

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
  protected getValue(address: AddressEntry): string|undefined {
    return address[this.property];
  }

  /**
   * Sets the value in the address that's associated with this component.
   */
  protected setValue(value: string|undefined, address: AddressEntry): void {
    address[this.property] = value;
  }

  private hasValue_(address = this.address_): boolean {
    return isValueNonEmpty(this.getValue(address));
  }
}

export class HonorificComponentUi extends AddressComponentUi {
  protected override readonly property = 'honorific';
}
export class CompanyNameComponentUi extends AddressComponentUi {
  protected override readonly property = 'companyName';
}
export class FullNamesComponentUi extends AddressComponentUi {
  protected override readonly property = 'fullName';
}
export class AddressLinesComponentUi extends AddressComponentUi {
  protected override readonly property = 'addressLines';
}
export class AddressLevel1ComponentUi extends AddressComponentUi {
  protected override readonly property = 'addressLevel1';
}
export class AddressLevel2ComponentUi extends AddressComponentUi {
  protected override readonly property = 'addressLevel2';
}
export class AddressLevel3ComponentUi extends AddressComponentUi {
  protected override readonly property = 'addressLevel3';
}
export class PostalCodeComponentUi extends AddressComponentUi {
  protected override readonly property = 'postalCode';
}
export class CountryCodeComponentUi extends AddressComponentUi {
  protected override readonly property = 'countryCode';
}
export class SortingCodeComponentUi extends AddressComponentUi {
  protected override readonly property = 'sortingCode';
}
export class PhoneComponentUi extends AddressComponentUi {
  protected override readonly property = 'phoneNumber';
}
export class EmailComponentUi extends AddressComponentUi {
  protected override readonly property = 'emailAddress';
}
