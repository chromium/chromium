// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function isValueEmpty(value: string|undefined): boolean {
  return value === undefined || value === '';
}

/**
 * The base class for data behind an address component. It exposes the `value`
 * property, which is how interface controls (e.g. input) communicate with it.
 */
export class AddressComponentUi {
  private readonly fieldType_: chrome.autofillPrivate.ServerFieldType;
  private readonly originalValue_?: string;
  private readonly existingAddress_: boolean;
  private readonly skipValidation_: boolean;
  private addressFields_:
      Map<chrome.autofillPrivate.ServerFieldType, string|undefined>;
  private isValidatable_: boolean;
  readonly isTextarea: boolean;
  readonly isRequired: boolean;
  readonly label: string;
  readonly additionalClassName: string;

  constructor(
      addressFields:
          Map<chrome.autofillPrivate.ServerFieldType, string|undefined>,
      originalFields:
          Map<chrome.autofillPrivate.ServerFieldType, string|undefined>|
      undefined,
      fieldType: chrome.autofillPrivate.ServerFieldType,
      label: string,
      additionalClassName: string = '',
      isTextarea: boolean = false,
      skipValidation: boolean = false,
      isRequired: boolean = false,
  ) {
    this.addressFields_ = addressFields;
    this.existingAddress_ = originalFields !== undefined;
    this.originalValue_ = originalFields?.get(fieldType);
    this.fieldType_ = fieldType;
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
    if (this.existingAddress_) {
      // "dont make it worse" validation for existing addresses:
      // consider a field valid as long as it is equal to the original value,
      // whether it is valid or not.
      if (this.originalValue_ === this.value ||
          (isValueEmpty(this.originalValue_) && isValueEmpty(this.value))) {
        return true;
      }
    }
    return !this.isRequired || !!this.value;
  }

  get value(): string|undefined {
    return this.addressFields_.get(this.fieldType_);
  }

  set value(value: string|undefined) {
    const changed = value !== this.value;
    this.addressFields_.set(this.fieldType_, value);
    if (changed) {
      this.onValueUpdateListener?.();
    }
  }

  get hasValue(): boolean {
    return !isValueEmpty(this.value);
  }

  makeValidatable(): void {
    this.isValidatable_ = true;
  }
}
