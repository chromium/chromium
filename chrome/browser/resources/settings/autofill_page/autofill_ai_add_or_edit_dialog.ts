// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-ai-add-or-edit-dialog' is the dialog that
 * allows adding and editing entity instances for Autofill AI.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import '../settings_shared.css.js';
import './passwords_shared.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './autofill_ai_add_or_edit_dialog.html.js';
import type {CountryDetailManagerProxy} from './country_detail_manager_proxy.js';
import {CountryDetailManagerProxyImpl} from './country_detail_manager_proxy.js';
import type {EntityDataManagerProxy} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

type AttributeInstance = chrome.autofillPrivate.AttributeInstance;
type AttributeType = chrome.autofillPrivate.AttributeType;
const AttributeTypeDataType = chrome.autofillPrivate.AttributeTypeDataType;
type AttributeTypeDataType = chrome.autofillPrivate.AttributeTypeDataType;
type CountryEntry = chrome.autofillPrivate.CountryEntry;
type DateValue = chrome.autofillPrivate.DateValue;
type EntityInstance = chrome.autofillPrivate.EntityInstance;

export interface SettingsAutofillAiAddOrEditDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SettingsAutofillAiSectionElementBase = I18nMixin(PolymerElement);

export class SettingsAutofillAiAddOrEditDialogElement extends
    SettingsAutofillAiSectionElementBase {
  static get is() {
    return 'settings-autofill-ai-add-or-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
         The entity instance to be modified. If this is an "add" dialog, the
         entity instance has only a type, but no attribute instances or guid.
       */
      entityInstance: {
        type: Object,
        value: null,
      },

      dialogTitle: {
        type: String,
        value: '',
      },

      attributeTypeDataTypeEnum_: {
        type: Object,
        value: AttributeTypeDataType,
      },

      /**
         Complete list of attribute instances that are associated with the
         current entity instance. If this is an "edit" dialog, some attribute
         instances are populated with their already existing values.
       */
      completeAttributeInstanceList_: {
        type: Array,
        computed: 'computeCompleteAttributeInstanceList_(countryList_, ' +
            'completeAttributeTypesList_)',
      },

      /**
         The list of all countries that should be displayed in a <select>
         element for a country field.
       */
      countryList_: {
        type: Array,
        value: () => [],
      },

      /**
         Complete list of attribute types that are associated with the
         current entity type.
       */
      completeAttributeTypesList_: {
        type: Array,
        value: () => [],
      },

      /**
         True if all fields are empty. The first validation occurs when the user
         clicks the "Save" button for the first time. Subsequent validations
         occur any time an input field is changed. If true, the "Save" button
         is disabled and an error message is displayed.
       */
      allFieldsAreEmpty_: {
        type: Boolean,
        value: false,
      },

      /**
         False if the form is invalid. The first validation occurs when the user
         clicks the "Save" button for the first time. Subsequent validations
         occur any time an input field is changed. If false, the "Save" button
         is disabled and an error message is displayed.
       */
      canSave_: {
        type: Boolean,
        value: true,
      },

      userClickedSaveButton_: {
        type: Boolean,
        value: false,
      },

      months_: {
        type: Array,
        // [1, 2, ..., 12]
        value: Array.from({length: 12}, (_, i) => i + 1).map(String),
      },

      days_: {
        type: Array,
        // There are always 31 days, regardless of month and year. This is an
        // acceptable trade-off.
        // [1, 2, ..., 31]
        value: Array.from({length: 31}, (_, i) => i + 1).map(String),
      },

      years_: {
        type: Array,
        value: () => {
          const currentYear: number = (new Date()).getFullYear();
          const firstYear: number = currentYear - 90;
          const lastYear: number = currentYear + 15;
          // [lastYear, ..., firstYear] (decreasing order)
          return Array
              .from(
                  {length: lastYear - firstYear + 1},
                  (_, index) => lastYear - index)
              .map(String);
        },
      },
    };
  }

  declare entityInstance: EntityInstance|null;
  declare dialogTitle: string;
  declare private completeAttributeInstanceList_: AttributeInstance[];
  declare private countryList_: CountryEntry[];
  declare private completeAttributeTypesList_: AttributeType[];
  declare private allFieldsAreEmpty_: boolean;
  declare private canSave_: boolean;
  declare private userClickedSaveButton_: boolean;
  declare private months_: string[];
  declare private days_: string[];
  declare private years_: string[];

  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();
  private countryDetailManager_: CountryDetailManagerProxy =
      CountryDetailManagerProxyImpl.getInstance();

  override async connectedCallback(): Promise<void> {
    super.connectedCallback();

    this.countryList_ = await this.countryDetailManager_.getCountryList(
        /*forAccountStorage=*/ false);

    assert(this.entityInstance);
    this.completeAttributeTypesList_ =
        await this.entityDataManager_.getAllAttributeTypesForEntityTypeName(
            this.entityInstance.type.typeName);

    // TODO(crbug.com/407794687): Decide whether the code should show a spinner
    // instead of delaying the display of the dialog. Keep this decision
    // consistent with all Autofill dialogs (Autofill Ai, Autofill, Payments,
    // etc.).
    // Open the modal only after all the properties are computed.
    this.$.dialog.showModal();
  }

  private computeCompleteAttributeInstanceList_(): AttributeInstance[] {
    if (this.countryList_.length === 0 ||
        this.completeAttributeTypesList_.length === 0) {
      return [];
    }

    return this.completeAttributeTypesList_.map(attributeType => {
      assert(this.entityInstance);
      const existingAttributeInstance =
          this.entityInstance.attributeInstances.find(
              existingAttributeInstance =>
                  existingAttributeInstance.type.typeName ===
                  attributeType.typeName);
      this.convertCountryAttributeInstance_(existingAttributeInstance);

      return {
        type: attributeType,
        value: existingAttributeInstance?.value ||
            (attributeType.dataType === AttributeTypeDataType.DATE ? {
                 month: '',
                 day: '',
                 year: '',
               } :
                                                                     ''),
      };
    });
  }

  private convertCountryAttributeInstance_(
      attributeInstance: AttributeInstance|undefined): void {
    if (!attributeInstance) {
      return;
    }
    // If `entityInstance` has a value stored for the country attribute, the
    // value will be the country name, not the country code. I.e. The value will
    // be "Germany", not "DE". On the other hand, the value stored into
    // `completeAttributeInstanceList_` should be the country code, not the
    // country name. I.e. The value should be "DE", not "Germany".
    // This logic exists because of a trade-off in the C++ autofill private API,
    // that has to call `EntityInstance::GetCompleteInfo()`, instead of
    // `EntityInstance::GetRawInfo()`.
    if (attributeInstance.type.dataType === AttributeTypeDataType.COUNTRY) {
      // TODO(crbug.com/403312087): Remove comment and exclamation marks once
      // the <hr> TODO below is solved.
      // The find operation will always find a match. Currently, the only entry
      // that doesn't have a name or a country code is the separator.
      attributeInstance.value = this.countryList_
                                    .find(
                                        country => attributeInstance.value ===
                                            country.name)!.countryCode!;
    }
  }

  private isDataType_(
      attributeInstance: AttributeInstance,
      dataType: AttributeTypeDataType): boolean {
    return attributeInstance.type.dataType === dataType;
  }


  private getCountryCode_(country: CountryEntry): string {
    // In case there is no country code, the string does not matter as long as
    // it is not empty and does not collide with any other country code.
    return country.countryCode || 'SEPARATOR';
  }

  private isCountrySeparator_(country: CountryEntry): boolean {
    return !country.countryCode;
  }


  private getCountryName_(country: CountryEntry): string {
    // TODO(crbug.com/403312087): Use <hr> as a separator, instead of hacking
    // the separator like this. To accommodate this, potentially refactor the
    // `CountryDetailManagerProxy` to return separately the current country and
    // the list of all countries. Do the same for regular Autofill.
    return country.name || '------';
  }

  private getMonthName_(month: string): string {
    const date = new Date();
    // `date` contains the current month, day and year. This becomes problematic
    // if the current day is 31, and the month is overridden to February (for
    // example), because `date` will overflow into March, because February 31st
    // doesn't exist.
    // Therefore, the code also has to override the day, to a day that is
    // present in all months.
    // Moreover, the day shouldn't be at the beginning of the month. If the code
    // sets the day to 1, and the month to January, the date will be January 1st
    // in UTC, but December 31st in Pacific Time.
    date.setDate(10);
    date.setMonth(Number(month) - 1);
    const formatter = new Intl.DateTimeFormat(
        document.documentElement.lang, {month: 'short'});
    return formatter.format(date);
  }


  private isCountrySelected_(
      attributeInstance: AttributeInstance, country: CountryEntry): boolean {
    return attributeInstance.value === this.getCountryCode_(country);
  }

  private isMonthSelected_(attributeInstance: AttributeInstance, month: string):
      boolean {
    return (attributeInstance.value as DateValue).month === month;
  }

  private isDaySelected_(attributeInstance: AttributeInstance, day: string):
      boolean {
    return (attributeInstance.value as DateValue).day === day;
  }

  private isYearSelected_(attributeInstance: AttributeInstance, year: string):
      boolean {
    return (attributeInstance.value as DateValue).year === year;
  }


  private onCountrySelectChange_(e: DomRepeatEvent<AttributeInstance>): void {
    this.completeAttributeInstanceList_[e.model.index].value =
        (e.target as HTMLSelectElement).value;
    this.onAttributeInstanceFieldInput_(e);
  }

  private onMonthSelectChange_(e: DomRepeatEvent<AttributeInstance>): void {
    (this.completeAttributeInstanceList_[e.model.index].value as DateValue)
        .month = (e.target as HTMLSelectElement).value;
    this.notifyPath(
        `completeAttributeInstanceList_.${e.model.index}.value.month`);
    this.onAttributeInstanceFieldInput_(e);
  }

  private onDaySelectChange_(e: DomRepeatEvent<AttributeInstance>): void {
    (this.completeAttributeInstanceList_[e.model.index].value as DateValue)
        .day = (e.target as HTMLSelectElement).value;
    this.notifyPath(
        `completeAttributeInstanceList_.${e.model.index}.value.day`);
    this.onAttributeInstanceFieldInput_(e);
  }

  private onYearSelectChange_(e: DomRepeatEvent<AttributeInstance>): void {
    (this.completeAttributeInstanceList_[e.model.index].value as DateValue)
        .year = (e.target as HTMLSelectElement).value;
    this.notifyPath(
        `completeAttributeInstanceList_.${e.model.index}.value.year`);
    this.onAttributeInstanceFieldInput_(e);
  }



  private isExistingYearOutOfBounds_(
      attributeInstance: AttributeInstance, years: string[]): boolean {
    const year = this.getExistingYear_(attributeInstance);
    return year.length > 0 && !years.includes(year);
  }

  private getExistingYear_(attributeInstance: AttributeInstance): string {
    return (attributeInstance.value as DateValue).year;
  }

  /**
   * Returns true if the date is invalid. A date is invalid either if it is
   * incomplete (i.e. only some of the month, day, year selectors are empty), or
   * if the combination of month, day, year is invalid (i.e. 30th of February
   * 2020 is invalid).
   * Returns false if month, day, year are all empty, or if the combination of
   * month, day, year is complete and valid.
   * The first validation occurs when the user clicks the "Save" button for the
   * first time. Subsequent validations occur any time a field is changed.
   */
  private isDateInvalid_(attributeInstance: AttributeInstance): boolean {
    if (attributeInstance.type.dataType !== AttributeTypeDataType.DATE ||
        !this.userClickedSaveButton_) {
      return false;
    }
    const value: DateValue = attributeInstance.value as DateValue;
    const month = value.month;
    const day = value.day;
    const year = value.year;

    const allEmpty =
        month.length === 0 && day.length === 0 && year.length === 0;
    const someEmpty =
        month.length === 0 || day.length === 0 || year.length === 0;

    if (allEmpty) {
      // The date is valid because month, day, year are all empty.
      return false;
    }
    if (someEmpty) {
      // The date is invalid because it is incomplete.
      return true;
    }

    // The date is complete. Check whether the combination of month, day, year
    // is valid. I.e. 30th of February 2020 is invalid.
    // `monthIndex` is indexed from 0, while `month` is indexed from 1.
    const date = new Date(+year, /*monthIndex=*/ +month - 1, +day);
    // If the combination of month, day, year is invalid, then `date` will
    // overflow into the next month.
    return (date.getFullYear() !== +year) || (date.getMonth() !== +month - 1) ||
        (date.getDate() !== +day);
  }

  /**
   * Returns true if the value is not empty and it is not made out only of
   * whitespaces.
   * For dates, at least one of month, day and year has to be not empty. An
   * incomplete date is not an empty field.
   */
  private isAttributeInstanceNotEmpty(attributeInstance: AttributeInstance):
      boolean {
    if (attributeInstance.type.dataType === AttributeTypeDataType.DATE) {
      const value: DateValue = attributeInstance.value as DateValue;
      return value.month.trim().length > 0 || value.day.trim().length > 0 ||
          value.year.trim().length > 0;
    }
    return (attributeInstance.value as string).trim().length > 0;
  }

  private onAttributeInstanceFieldInput_(_e: Event): void {
    if (this.userClickedSaveButton_) {
      this.validateForm_();
    }
  }

  private validateForm_(): void {
    this.allFieldsAreEmpty_ = !this.completeAttributeInstanceList_.some(
        attributeInstance =>
            this.isAttributeInstanceNotEmpty(attributeInstance));
    const invalidDateExists = this.completeAttributeInstanceList_.some(
        attributeInstance => this.isDateInvalid_(attributeInstance));
    this.canSave_ = !this.allFieldsAreEmpty_ && !invalidDateExists;
  }

  private onCancelClick_(): void {
    this.$.dialog.cancel();
  }

  private onConfirmClick_(): void {
    this.userClickedSaveButton_ = true;
    this.validateForm_();
    if (this.canSave_) {
      this.dispatchEvent(new CustomEvent('autofill-ai-add-or-edit-done', {
        bubbles: true,
        composed: true,
        detail: {
          ...this.entityInstance,
          attributeInstances: this.completeAttributeInstanceList_.filter(
              attributeInstance =>
                  this.isAttributeInstanceNotEmpty(attributeInstance)),
        },
      }));
      this.$.dialog.close();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-ai-add-or-edit-dialog':
        SettingsAutofillAiAddOrEditDialogElement;
  }
}

customElements.define(
    SettingsAutofillAiAddOrEditDialogElement.is,
    SettingsAutofillAiAddOrEditDialogElement);
