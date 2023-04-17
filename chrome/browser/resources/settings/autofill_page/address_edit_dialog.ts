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
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {flush, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './address_edit_dialog.html.js';
import * as uiComponents from './address_edit_dialog_components.js';

const SANCTOINED_COUNTRY_CODES: readonly string[] =
    Object.freeze(['CU', 'IR', 'KP', 'SD', 'SY']);

export interface SettingsAddressEditDialogElement {
  $: {
    accountSourceNotice: HTMLElement,
    cancelButton: CrButtonElement,
    country: HTMLSelectElement,
    dialog: CrDialogElement,
    saveButton: CrButtonElement,
  };
}

type CountryEntry = chrome.autofillPrivate.CountryEntry;
type AddressEntry = chrome.autofillPrivate.AddressEntry;
type AccountInfo = chrome.autofillPrivate.AccountInfo;
type AddressComponents = chrome.autofillPrivate.AddressComponents;
const AddressSource = chrome.autofillPrivate.AddressSource;
const AddressField = chrome.autofillPrivate.AddressField;
const SettingsAddressEditDialogElementBase = I18nMixin(PolymerElement);

export class SettingsAddressEditDialogElement extends
    SettingsAddressEditDialogElementBase {
  static get is() {
    return 'settings-address-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      address: Object,
      accountInfo: Object,

      title_: String,
      validationError_: String,

      countries_: Array,

      /**
       * Updates the address wrapper.
       */
      countryCode_: {
        type: String,
        observer: 'onUpdateCountryCode_',
      },

      components_: Array,
      phoneNumber_: String,
      email_: String,
      canSave_: Boolean,

      isAccountAddress_: {
        type: Boolean,
        computed: 'isAddressStoredInAccount_(address, accountInfo)',
        value: false,
      },

      accountAddressSourceNotice_: {
        type: String,
        computed: 'getAccountAddressSourceNotice_(address, accountInfo)',
      },

      /**
       * True if honorifics are enabled.
       */
      showHonorific_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showHonorific');
        },
      },
    };
  }

  address: AddressEntry;
  accountInfo?: AccountInfo;

  /**
   * Original address is a snapshot of the address made at initialization,
   * it is a referce for soft (or "dont make it worse") validation, which
   * basically means skipping validation for fields that are already invalid.
   */
  private originalAddress_?: AddressEntry;
  private title_: string;
  private validationError_?: string;
  private countries_: CountryEntry[];
  private countryCode_: string|undefined;
  private components_: Array<Array<uiComponents.AddressComponentUi<unknown>>> =
      [];
  private canSave_: boolean;
  private isAccountAddress_: boolean;
  private showHonorific_: boolean;
  private countryInfo_: CountryDetailManager =
      CountryDetailManagerImpl.getInstance();

  override connectedCallback(): void {
    super.connectedCallback();

    assert(this.address);

    this.countryInfo_.getCountryList().then(countryList => {
      if (this.address.guid && this.address.metadata !== undefined &&
          this.address.metadata.source === AddressSource.ACCOUNT) {
        // TODO(crbug.com/1432505): remove temporary sanctioned countries
        // filtering.
        countryList = countryList.filter(
            country => !!country.countryCode &&
                !SANCTOINED_COUNTRY_CODES.includes(country.countryCode));
      }
      this.countries_ = countryList;

      const isEditingExistingAddress = !!this.address.guid;
      this.title_ = this.i18n(
          isEditingExistingAddress ? 'editAddressTitle' : 'addAddressTitle');
      this.originalAddress_ =
          isEditingExistingAddress ? structuredClone(this.address) : undefined;

      microTask.run(() => {
        if (Object.keys(this.address).length === 0 && countryList.length > 0) {
          // If the address is completely empty, the dialog is creating a new
          // address. The first address in the country list is what we suspect
          // the user's country is.
          this.address.countryCode = countryList[0].countryCode;
        }
        if (this.countryCode_ === this.address.countryCode) {
          this.updateAddressComponents_();
        } else {
          this.countryCode_ = this.address.countryCode;
        }
      });
    });

    // Open is called on the dialog after the address wrapper has been
    // updated.
  }

  private fire_(eventName: string, detail?: any): void {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  /**
   * Updates the wrapper that represents this address in the country's format.
   */
  private updateAddressComponents_(): void {
    // Default to the last country used if no country code is provided.
    const countryCode = this.countryCode_ || this.countries_[0].countryCode;
    this.countryInfo_.getAddressFormat(countryCode as string).then(format => {
      this.address.languageCode = format.languageCode;
      // TODO(crbug.com/1408117): validation is performed for addresses from
      // the user account only now, this flag should be removed when it
      // becomes the only type of addresses
      const skipValidation = !this.isAccountAddress_;

      this.components_ = [];
      for (const row of format.components) {
        // If this is the name field, add a honorific title row before it.
        if (row.row[0].field === AddressField.FULL_NAME &&
            this.showHonorific_) {
          this.components_.push(
              [new ADDRESS_FIELD_COMPONENT_UI[AddressField.HONORIFIC](
                  this.address, this.originalAddress_,
                  this.i18n('honorificLabel'), 'long')]);
        }

        this.components_.push(row.row.map(
            component => new ADDRESS_FIELD_COMPONENT_UI[component.field](
                this.address, this.originalAddress_, component.fieldName,
                component.isLongField ? 'long' : '',
                component.field === AddressField.ADDRESS_LINES, skipValidation,
                component.isRequired)));
      }

      // Phone and email do not come in the address format as fields, but
      // should be editable and saveable in the resulting address.
      this.components_.push([
        new uiComponents.PhoneComponentUi(
            this.address, this.originalAddress_, this.i18n('addressPhone'),
            'last-row'),
        new uiComponents.EmailComponentUi(
            this.address, this.originalAddress_, this.i18n('addressEmail'),
            'long last-row'),
      ]);

      // Because of potentially added honorific field the resulting components
      // structure my be different from the original format, that is why
      // the onValueUpdateListener with row/col indices is updated after.
      for (let rowIndex = 0; rowIndex < this.components_.length; ++rowIndex) {
        const row = this.components_[rowIndex];
        for (let colIndex = 0; colIndex < row.length; ++colIndex) {
          this.components_[rowIndex][colIndex].onValueUpdateListener =
              this.notifyComponentValidity_.bind(this, rowIndex, colIndex);
        }
      }

      // Flush dom before resize and savability updates.
      flush();

      this.updateCanSave_();

      this.fire_('on-update-address-wrapper');  // For easier testing.

      if (!this.$.dialog.open) {
        this.$.dialog.showModal();
      }
    });
  }

  /**
   * Determines whether component with specified validation property
   * should be rendered as invalid in the template.
   */
  private isVisuallyInvalid_(isValidatable: boolean, isValid: boolean):
      boolean {
    return isValidatable && !isValid;
  }

  /**
   * Makes component's potentially invalid state visible, it makes
   * the component validatable and notifies the template engine.
   * The component is addressed by row/col to leverage Polymer's notifications.
   */
  private notifyComponentValidity_(row: number, col: number): void {
    this.components_[row][col].makeValidatable();

    const componentReference = `components_.${row}.${col}`;
    this.notifyPath(componentReference + '.isValidatable');
    this.notifyPath(componentReference + '.isValid');

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

  private updateCanSave_(): void {
    this.validationError_ = '';

    if ((!this.countryCode_ && this.hasAnyValue_()) ||
        (this.countryCode_ &&
         (!this.hasInvalidComponent_() ||
          this.hasUncoveredInvalidComponent_()))) {
      this.canSave_ = true;
      this.fire_('on-update-can-save');  // For easier testing.
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
    this.fire_('on-update-can-save');  // For easier testing.
  }

  private getCode_(country: CountryEntry): string {
    return country.countryCode || 'SPACER';
  }

  private getName_(country: CountryEntry): string {
    return country.name || '------';
  }

  private isDivision_(country: CountryEntry): boolean {
    return !country.countryCode;
  }

  private isAddressStoredInAccount_(): boolean {
    if (this.address.guid) {
      return this.address.metadata !== undefined &&
          this.address.metadata.source === AddressSource.ACCOUNT;
    }

    return this.accountInfo !== undefined &&
        this.accountInfo.isEligibleForAddressAccountStorage;
  }

  private getAccountAddressSourceNotice_(): string|undefined {
    if (this.accountInfo) {
      return this.i18n(
          this.address.guid ? 'editAccountAddressSourceNotice' :
                              'newAccountAddressSourceNotice',
          this.accountInfo.email);
    }

    return undefined;
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

  private onCancelClick_(): void {
    this.$.dialog.cancel();
  }

  /**
   * Handler for tapping the save button.
   */
  private onSaveButtonClick_(): void {
    this.notifyValidity_();

    this.updateCanSave_();
    if (!this.canSave_) {
      return;
    }

    // Set a default country if none is set.
    if (!this.address.countryCode) {
      this.address.countryCode = this.countries_[0].countryCode;
    }

    this.fire_('save-address', this.address);
    this.$.dialog.close();
  }

  /**
   * Syncs the country code back to the address and rebuilds the address
   * components for the new location.
   */
  private onUpdateCountryCode_(countryCode: string|undefined): void {
    this.address.countryCode = countryCode;
    this.updateAddressComponents_();
  }

  private onCountryChange_(): void {
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

export const ADDRESS_FIELD_COMPONENT_UI: Record<
    chrome.autofillPrivate.AddressField,
    typeof uiComponents.StringComponentUi|
    typeof uiComponents.ArrayStringComponentUi> = {
  [AddressField.HONORIFIC]: uiComponents.HonorificComponentUi,
  [AddressField.COMPANY_NAME]: uiComponents.CompanyNameComponentUi,
  [AddressField.FULL_NAME]: uiComponents.FullNamesComponentUi,
  [AddressField.ADDRESS_LINES]: uiComponents.AddressLinesComponentUi,
  [AddressField.ADDRESS_LEVEL_1]: uiComponents.AddressLevel1ComponentUi,
  [AddressField.ADDRESS_LEVEL_2]: uiComponents.AddressLevel2ComponentUi,
  [AddressField.ADDRESS_LEVEL_3]: uiComponents.AddressLevel3ComponentUi,
  [AddressField.POSTAL_CODE]: uiComponents.PostalCodeComponentUi,
  [AddressField.COUNTRY_CODE]: uiComponents.CountryCodeComponentUi,
  [AddressField.SORTING_CODE]: uiComponents.SortingCodeComponentUi,
};

export interface CountryDetailManager {
  /**
   * Gets the list of available countries.
   * The default country will be first, followed by a separator, followed by
   * an alphabetized list of countries available.
   */
  getCountryList(): Promise<CountryEntry[]>;

  /**
   * Gets the address format for a given country code.
   */
  getAddressFormat(countryCode: string): Promise<AddressComponents>;
}

/**
 * Default implementation. Override for testing.
 */
export class CountryDetailManagerImpl implements CountryDetailManager {
  getCountryList(): Promise<CountryEntry[]> {
    return chrome.autofillPrivate.getCountryList();
  }

  getAddressFormat(countryCode: string): Promise<AddressComponents> {
    return chrome.autofillPrivate.getAddressComponents(countryCode);
  }

  static getInstance(): CountryDetailManager {
    return instance || (instance = new CountryDetailManagerImpl());
  }

  static setInstance(obj: CountryDetailManager): void {
    instance = obj;
  }
}

let instance: CountryDetailManager|null = null;
