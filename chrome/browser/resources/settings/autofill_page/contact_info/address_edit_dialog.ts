// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'address-edit-dialog' is the dialog that allows editing a saved
 * address.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CountryDetailManagerProxy} from '../country_detail_manager_proxy.js';
import {CountryDetailManagerProxyImpl} from '../country_detail_manager_proxy.js';

import {getCss} from './address_edit_dialog.css.js';
import {getHtml} from './address_edit_dialog.html.js';
import * as uiComponents from './address_edit_dialog_components.js';

export interface SettingsAddressEditDialogElement {
  $: {
    accountRecordTypeNotice: HTMLElement,
    cancelButton: CrButtonElement,
    country: HTMLSelectElement,
    dialog: CrDialogElement,
    saveButton: CrButtonElement,
  };
}

type CountryEntry = chrome.autofillPrivate.CountryEntry;
type AddressEntry = chrome.autofillPrivate.AddressEntry;
type AccountInfo = chrome.autofillPrivate.AccountInfo;
const AddressRecordType = chrome.autofillPrivate.AddressRecordType;
const FieldType = chrome.autofillPrivate.FieldType;
const SettingsAddressEditDialogElementBase = I18nMixinLit(CrLitElement);

export class SettingsAddressEditDialogElement extends
    SettingsAddressEditDialogElementBase {
  static get is() {
    return 'settings-address-edit-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      address: {type: Object},
      accountInfo: {type: Object},

      title_: {type: String},
      validationError_: {type: String},

      countries_: {type: Array},

      /**
       * Updates the address wrapper.
       */
      countryCode_: {type: String},

      components_: {type: Array},

      canSave_: {type: Boolean},

      isAccountAddress_: {type: Boolean},

      accountAddressRecordTypeNotice_: {type: String},
    };
  }

  accessor address: AddressEntry = {fields: []};
  accessor accountInfo: AccountInfo|undefined;

  /**
   * Original address is a snapshot of the address made at initialization,
   * it is a referce for soft (or "dont make it worse") validation, which
   * basically means skipping validation for fields that are already invalid.
   */
  protected accessor title_: string = '';
  protected accessor validationError_: string|undefined;
  protected accessor countries_: CountryEntry[] = [];
  protected accessor countryCode_: string = '';
  protected accessor components_: uiComponents.AddressComponentUi[][] = [];
  protected accessor canSave_: boolean = false;
  protected accessor isAccountAddress_: boolean = false;
  protected accessor accountAddressRecordTypeNotice_: string = '';

  private addressFields_:
      Map<chrome.autofillPrivate.FieldType, string|undefined> = new Map();
  private originalAddressFields_?:
      Map<chrome.autofillPrivate.FieldType, string|undefined>;
  private countryDetailManager_: CountryDetailManagerProxy =
      CountryDetailManagerProxyImpl.getInstance();

  override connectedCallback(): void {
    super.connectedCallback();

    assert(this.address);
    for (const entry of this.address.fields) {
      this.addressFields_.set(entry.type, entry.value);
    }

    const forAccountStorage = !!this.address.guid &&
        this.address.metadata !== undefined &&
        this.address.metadata.recordType === AddressRecordType.ACCOUNT;
    this.countryDetailManager_.getCountryList(forAccountStorage)
        .then(countryList => {
          this.countries_ = countryList;

          const isEditingExistingAddress = !!this.address.guid;
          this.title_ = this.i18n(
              isEditingExistingAddress ? 'editAddressTitle' :
                                         'addAddressTitle');
          this.originalAddressFields_ = isEditingExistingAddress ?
              new Map(this.addressFields_) :
              undefined;

          queueMicrotask(() => {
            const countryField =
                this.addressFields_.get(FieldType.ADDRESS_HOME_COUNTRY);
            if (!countryField) {
              assert(countryList.length > 0);
              // If the address is completely empty, the dialog is creating a
              // new address. The first address in the country list is what we
              // suspect the user's country is.
              this.addressFields_.set(
                  FieldType.ADDRESS_HOME_COUNTRY, countryList[0].countryCode);
            }
            this.countryCode_ =
                this.addressFields_.get(FieldType.ADDRESS_HOME_COUNTRY) || '';
          });
        });

    // Open is called on the dialog after the address wrapper has been
    // updated.
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('address') ||
        changedProperties.has('accountInfo')) {
      this.isAccountAddress_ = this.isAddressStoredInAccount_();
      this.accountAddressRecordTypeNotice_ =
          this.getAccountAddressRecordTypeNotice_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('countries_')) {
      this.$.country.value = this.countryCode_;
    }
    if (changedPrivateProperties.has('countryCode_') &&
        changedPrivateProperties.get('countryCode_') !== undefined) {
      this.onCountryCodeChanged_();
    }
  }

  /**
   * Updates the wrapper that represents this address in the country's format.
   */
  private async updateAddressComponents_(): Promise<void> {
    // Default to the last country used if no country code is provided.
    const countryCode = this.countryCode_ || this.countries_[0].countryCode;
    const format = await this.countryDetailManager_.getAddressFormat(
        countryCode as string);
    this.address.languageCode = format.languageCode;
    // TODO(crbug.com/40253382): validation is performed for addresses
    // from the user account only now, this flag should be removed when it
    // becomes the only type of addresses
    const skipValidation = !this.isAccountAddress_;

    const components = format.components.map((componentRow, rowIndex) => {
      return componentRow.row.map((component, columnIndex) => {
        return new uiComponents.AddressComponentUi(
            this.addressFields_, this.originalAddressFields_, component.field,
            component.fieldName,
            this.notifyComponentValidity_.bind(this, rowIndex, columnIndex),
            component.isLongField ? 'long' : '',
            component.field === FieldType.ADDRESS_HOME_STREET_ADDRESS,
            skipValidation, component.isRequired);
      });
    });

    // Phone and email do not come in the address format as fields, but
    // should be editable and saveable in the resulting address.
    const contactsRowIndex = components.length;
    components.push([
      new uiComponents.AddressComponentUi(
          this.addressFields_, this.originalAddressFields_,
          FieldType.PHONE_HOME_WHOLE_NUMBER, this.i18n('addressPhone'),
          this.notifyComponentValidity_.bind(this, contactsRowIndex, 0),
          'last-row'),
      new uiComponents.AddressComponentUi(
          this.addressFields_, this.originalAddressFields_,
          FieldType.EMAIL_ADDRESS, this.i18n('addressEmail'),
          this.notifyComponentValidity_.bind(this, contactsRowIndex, 1),
          'long last-row'),
    ]);
    this.components_ = components;

    // Wait for dom update before resize and savability updates.
    await this.updateComplete;

    await this.updateCanSave_();

    this.fire('on-update-address-wrapper');  // For easier testing.

    if (!this.$.dialog.open) {
      this.$.dialog.showModal();
    }
  }

  /**
   * Determines whether component with specified validation property
   * should be rendered as invalid in the template.
   */
  protected isVisuallyInvalid_(isValidatable: boolean, isValid: boolean):
      boolean {
    return isValidatable && !isValid;
  }

  /**
   * Makes component's potentially invalid state visible, it makes
   * the component validatable and notifies the template engine.
   * The component is addressed by row/col to leverage notifications.
   */
  private notifyComponentValidity_(row: number, col: number): void {
    this.components_[row][col].makeValidatable();
    this.requestUpdate();
    this.updateCanSave_();
  }

  /**
   * Notifies all components validity (see notifyComponentValidity_()).
   */
  private notifyValidity_(): void {
    this.components_.forEach((row, i) => {
      row.forEach((_col, j) => this.notifyComponentValidity_(i, j));
    });
  }

  private async updateCanSave_(): Promise<void> {
    this.validationError_ = '';

    if ((!this.countryCode_ && this.hasAnyValue_()) ||
        (this.countryCode_ &&
         (!this.hasInvalidComponent_() ||
          this.hasUncoveredInvalidComponent_()))) {
      this.canSave_ = true;
      await this.updateComplete;
      this.fire('on-update-can-save');  // For easier testing.
      return;
    }

    if (this.isAccountAddress_) {
      const nInvalid = this.countInvalidComponent_();
      if (nInvalid === 1) {
        this.validationError_ = this.i18n('editAddressRequiredFieldError');
      } else if (nInvalid > 1) {
        this.validationError_ = this.i18n('editAddressRequiredFieldsError');
      }
    }

    this.canSave_ = false;
    await this.updateComplete;
    this.fire('on-update-can-save');  // For easier testing.
  }

  protected onValueChanged_(e: CustomEvent<{value: string}>): void {
    const target = e.currentTarget as HTMLElement;
    const row = Number(target.dataset['row']);
    const column = Number(target.dataset['column']);
    const component = this.components_[row][column];
    assert(component);
    component.value = e.detail.value;
  }

  protected getCode_(country: CountryEntry): string {
    return country.countryCode || 'SPACER';
  }

  protected getName_(country: CountryEntry): string {
    return country.name || '------';
  }

  protected isDivision_(country: CountryEntry): boolean {
    return !country.countryCode;
  }

  protected getPhoneNumberInputClass_(
      fieldType: chrome.autofillPrivate.FieldType): string {
    if (fieldType ===
        chrome.autofillPrivate.FieldType.PHONE_HOME_WHOLE_NUMBER) {
      return 'phone-number-input';
    }
    return '';
  }

  private isAddressStoredInAccount_(): boolean {
    if (this.address.guid) {
      return this.address.metadata !== undefined &&
          this.address.metadata.recordType === AddressRecordType.ACCOUNT;
    }

    return !!this.accountInfo?.isEligibleForAddressAccountStorage;
  }

  private getAccountAddressRecordTypeNotice_(): string {
    if (this.accountInfo) {
      return this.i18n(
          this.address.guid ? 'editAccountAddressRecordTypeNotice' :
                              'newAccountAddressRecordTypeNotice',
          this.accountInfo.email);
    }

    return '';
  }

  /**
   * Tells whether at least one address component (except country)
   * has a non empty value.
   */
  private hasAnyValue_(): boolean {
    return this.components_.flat().some(component => component.hasValue);
  }

  /**
   * Tells whether at least one address component (except country) is not valid.
   */
  private hasInvalidComponent_(): boolean {
    return this.countInvalidComponent_() > 0;
  }

  /**
   * Counts how many invalid address componets (except country) are in the form.
   */
  private countInvalidComponent_(): number {
    return this.components_.flat()
        .filter(component => !component.isValid)
        .length;
  }

  /**
   * Tells whether at least one address component (except country)
   * is not valid and is not validatable also, i.e. its invalid state is
   * not visible to the user.
   */
  private hasUncoveredInvalidComponent_(): boolean {
    return this.components_.flat().some(
        component => !component.isValid && !component.isValidatable);
  }

  protected onCancelClick_(): void {
    chrome.metricsPrivate.recordBoolean(
        'Autofill.Settings.EditAddress',
        /*confirmed=*/ false);
    this.$.dialog.cancel();
  }

  /**
   * Handler for tapping the save button.
   */
  protected onSaveButtonClick_(): void {
    this.notifyValidity_();

    this.updateCanSave_();
    if (!this.canSave_) {
      return;
    }

    this.address.fields = [];
    this.addressFields_.forEach((value, key, _map) => {
      this.address.fields.push({type: key, value: value});
    });

    chrome.metricsPrivate.recordBoolean(
        'Autofill.Settings.EditAddress',
        /*confirmed=*/ true);
    this.fire('save-address', this.address);
    this.$.dialog.close();
  }

  private onCountryCodeChanged_(): void {
    this.updateAddressComponents_();
  }

  /**
   * Syncs the country code back to the address and rebuilds the address
   * components for the new location.
   */
  protected onCountryCodeSelectChange_(): void {
    this.addressFields_.set(
        FieldType.ADDRESS_HOME_COUNTRY, this.$.country.value);
    this.countryCode_ = this.$.country.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-address-edit-dialog': SettingsAddressEditDialogElement;
  }
}

customElements.define(
    SettingsAddressEditDialogElement.is, SettingsAddressEditDialogElement);
