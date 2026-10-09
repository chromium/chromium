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

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './autofill_ai_add_or_edit_dialog.css.js';
import {getHtml} from './autofill_ai_add_or_edit_dialog.html.js';
import type {CountryDetailManagerProxy} from './country_detail_manager_proxy.js';
import {CountryDetailManagerProxyImpl} from './country_detail_manager_proxy.js';
import type {EntityDataManagerProxy} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

type AttributeInstance = chrome.autofillPrivate.AttributeInstance;
type AttributeType = chrome.autofillPrivate.AttributeType;
type AttributeTypeDataType = chrome.autofillPrivate.AttributeTypeDataType;
type CountryEntry = chrome.autofillPrivate.CountryEntry;
type DateValue = chrome.autofillPrivate.DateValue;
type EntityInstance = chrome.autofillPrivate.EntityInstance;
type EntityType = chrome.autofillPrivate.EntityType;
type UpsertPassDetails = chrome.autofillPrivate.UpsertPassDetails;

export interface SettingsAutofillAiAddOrEditDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SettingsAutofillAiAddOrEditDialogElementBase = I18nMixinLit(CrLitElement);

export class SettingsAutofillAiAddOrEditDialogElement extends
    SettingsAutofillAiAddOrEditDialogElementBase {
  static get is() {
    return 'settings-autofill-ai-add-or-edit-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
         The entity instance to be modified. If this is an "add" dialog, the
         entity instance has only a type, but no attribute instances or guid.
       */
      entityInstance: {type: Object},

      dialogTitle: {type: String},

      /**
         Complete list of attribute instances that are associated with the
         current entity instance. If this is an "edit" dialog, some attribute
         instances are populated with their already existing values.
       */
      completeAttributeInstanceList_: {type: Array},

      /**
         The list of all countries that should be displayed in a <select>
         element for a country field.
       */
      countryList_: {type: Array},

      /**
         Complete list of attribute types that are associated with the
         current entity type.
       */
      completeAttributeTypesList_: {type: Array},

      /**
       *  User email associated with the account.
       */
      userEmail_: {type: String},

      /**
       * Footer text shown in the view, represented as a TrustedHTML object,
       * since the footer text can contain a link. If the object represents an
       * empty TrustedHTML, no footer text is shown.
       */
      footerText_: {type: String},

      /**
       * Details for upserting a public pass, including legal message lines and
       * context token.
       */
      upsertPassDetails: {type: Object},

      /**
         True if all fields are empty. The first validation occurs when the user
         clicks the "Save" button for the first time. Subsequent validations
         occur any time an input field is changed. If true, the "Save" button
         is disabled and an error message is displayed.
       */
      allFieldsAreEmpty_: {type: Boolean},

      /**
         False if the form is invalid. The first validation occurs when the user
         clicks the "Save" button for the first time. Subsequent validations
         occur any time an input field is changed. If false, the "Save" button
         is disabled and an error message is displayed.
       */
      canSave_: {type: Boolean},

      userClickedSaveButton_: {type: Boolean},

      /**
         Holds the error to display (or empty string if valid).
       */
      validationError_: {type: String},

      months_: {type: Array},

      days_: {type: Array},

      years_: {type: Array},

      enableSavePrivatePassesToWallet_: {type: Boolean},

      isWalletPassBranding2026Enabled_: {type: Boolean},

      /**
       * True while waiting for the backend to respond from Wallet API call.
       */
      saveInProgress_: {type: Boolean},
    };
  }

  accessor entityInstance: EntityInstance|null = null;
  accessor dialogTitle: string = '';
  accessor upsertPassDetails: UpsertPassDetails|null = null;
  protected accessor completeAttributeInstanceList_: AttributeInstance[] = [];
  protected accessor countryList_: CountryEntry[] = [];
  private accessor completeAttributeTypesList_: AttributeType[] = [];
  private accessor allFieldsAreEmpty_: boolean = false;
  protected accessor canSave_: boolean = true;
  private accessor userClickedSaveButton_: boolean = false;
  protected accessor validationError_: string = '';
  // [1, 2, ..., 12]
  protected accessor months_: string[] =
      Array.from({length: 12}, (_, i) => i + 1).map(String);
  // There are always 31 days, regardless of month and year. This is an
  // acceptable trade-off.
  // [1, 2, ..., 31]
  protected accessor days_: string[] =
      Array.from({length: 31}, (_, i) => i + 1).map(String);
  protected accessor years_: string[];
  private accessor userEmail_: string = '';
  protected accessor footerText_: TrustedHTML = window.trustedTypes!.emptyHTML;
  private accessor enableSavePrivatePassesToWallet_: boolean =
      loadTimeData.getBoolean('enableAutofillAiWalletPrivatePasses');
  protected accessor isWalletPassBranding2026Enabled_: boolean =
      loadTimeData.getBoolean('isAutofillAiWalletPassBranding2026Enabled');
  protected accessor saveInProgress_: boolean = false;

  private requiredAttributeTypes_: AttributeType[] = [];
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();
  private countryDetailManager_: CountryDetailManagerProxy =
      CountryDetailManagerProxyImpl.getInstance();

  constructor() {
    super();

    const currentYear: number = (new Date()).getFullYear();
    const firstYear: number = currentYear - 90;
    const lastYear: number = currentYear + 15;
    // [lastYear, ..., firstYear] (decreasing order)
    this.years_ = Array
                      .from(
                          {length: lastYear - firstYear + 1},
                          (_, index) => lastYear - index)
                      .map(String);
  }

  override async connectedCallback(): Promise<void> {
    super.connectedCallback();
    assert(this.entityInstance);

    this.countryList_ = await this.countryDetailManager_.getCountryList(
        /*forAccountStorage=*/ false);

    const [attributeTypes, requiredAttributes] = await Promise.all([
      this.entityDataManager_.getAllAttributeTypesForEntityTypeName(
          this.entityInstance.type.typeName),
      this.entityDataManager_.getRequiredAttributeTypesForEntityTypeName(
          this.entityInstance.type.typeName),
    ]);

    this.completeAttributeTypesList_ = attributeTypes;
    this.requiredAttributeTypes_ = requiredAttributes;

    const accountInfo = await this.entityDataManager_.getAccountInfo();
    if (accountInfo && accountInfo.email) {
      this.userEmail_ = accountInfo.email;
    }

    if (loadTimeData.getBoolean('enableWalletDisclosureNoticePublicPass') &&
        !this.entityInstance.guid &&
        this.isPublicPass_(this.entityInstance.type) &&
        !this.upsertPassDetails) {
      this.upsertPassDetails =
          await this.entityDataManager_.getDetailsForUpsertPass();
    }

    // TODO(crbug.com/407794687): Decide whether the code should show a spinner
    // instead of delaying the display of the dialog. Keep this decision
    // consistent with all Autofill dialogs (Autofill Ai, Autofill, Payments,
    // etc.).
    // Open the modal only after all the properties are computed.
    this.$.dialog.showModal();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('countryList_') ||
        changedPrivateProperties.has('completeAttributeTypesList_')) {
      this.completeAttributeInstanceList_ =
          this.computeCompleteAttributeInstanceList_();
    }

    if (changedProperties.has('entityInstance') ||
        changedProperties.has('upsertPassDetails') ||
        changedPrivateProperties.has('userEmail_')) {
      this.footerText_ = this.computeFooterText_();
    }
  }

  private checkRequiredFields_(): boolean {
    if (this.requiredAttributeTypes_.length === 0) {
      return true;
    }

    // At least one of the attributes in the list must be non-empty.
    return this.requiredAttributeTypes_.some(req => {
      const attribute = this.completeAttributeInstanceList_.find(
          attr => attr.type.typeName === req.typeName);
      return attribute && this.isAttributeInstanceNotEmpty(attribute);
    });
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
        value: existingAttributeInstance ?
            structuredClone(existingAttributeInstance.value) :
            (attributeType.dataType ===
                     chrome.autofillPrivate.AttributeTypeDataType.DATE ?
                 {
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
    if (attributeInstance.type.dataType ===
        chrome.autofillPrivate.AttributeTypeDataType.COUNTRY) {
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

  protected isDataType_(
      attributeInstance: AttributeInstance,
      dataType: AttributeTypeDataType): boolean {
    return attributeInstance.type.dataType === dataType;
  }

  protected getCountryCode_(country: CountryEntry): string {
    // In case there is no country code, the string does not matter as long as
    // it is not empty and does not collide with any other country code.
    return country.countryCode || 'SEPARATOR';
  }

  protected isCountrySeparator_(country: CountryEntry): boolean {
    return !country.countryCode;
  }

  protected getCountryName_(country: CountryEntry): string {
    // TODO(crbug.com/403312087): Use <hr> as a separator, instead of hacking
    // the separator like this. To accommodate this, potentially refactor the
    // `CountryDetailManagerProxy` to return separately the current country and
    // the list of all countries. Do the same for regular Autofill.
    return country.name || '------';
  }

  protected getMonthName_(month: string): string {
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

  protected isCountrySelected_(
      attributeInstance: AttributeInstance, country: CountryEntry): boolean {
    return attributeInstance.value === this.getCountryCode_(country);
  }

  protected isMonthSelected_(
      attributeInstance: AttributeInstance, month: string): boolean {
    return (attributeInstance.value as DateValue).month === month;
  }

  protected isDaySelected_(attributeInstance: AttributeInstance, day: string):
      boolean {
    return (attributeInstance.value as DateValue).day === day;
  }

  protected isYearSelected_(attributeInstance: AttributeInstance, year: string):
      boolean {
    return (attributeInstance.value as DateValue).year === year;
  }

  protected onCountrySelectChange_(e: Event): void {
    const target = e.currentTarget as HTMLSelectElement;
    const index = Number(target.dataset['index']);
    this.completeAttributeInstanceList_[index].value = target.value;
    this.requestUpdate();
    this.onAttributeInstanceFieldInput_(e);
  }

  protected onMonthSelectChange_(e: Event): void {
    const target = e.currentTarget as HTMLSelectElement;
    const index = Number(target.dataset['index']);
    (this.completeAttributeInstanceList_[index].value as DateValue).month =
        target.value;
    this.requestUpdate();
    this.onAttributeInstanceFieldInput_(e);
  }

  protected onDaySelectChange_(e: Event): void {
    const target = e.currentTarget as HTMLSelectElement;
    const index = Number(target.dataset['index']);
    (this.completeAttributeInstanceList_[index].value as DateValue).day =
        target.value;
    this.requestUpdate();
    this.onAttributeInstanceFieldInput_(e);
  }

  protected onYearSelectChange_(e: Event): void {
    const target = e.currentTarget as HTMLSelectElement;
    const index = Number(target.dataset['index']);
    (this.completeAttributeInstanceList_[index].value as DateValue).year =
        target.value;
    this.requestUpdate();
    this.onAttributeInstanceFieldInput_(e);
  }

  protected onAttributeInstanceFieldValueChanged_(
      e: CustomEvent<{value: string}>): void {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    this.completeAttributeInstanceList_[index].value = e.detail.value;
    this.requestUpdate();
  }

  /**
   * Returns '*' if the field is required.
   */
  protected getRequiredIndicator_(attributeInstance: AttributeInstance):
      string {
    const isRequired = this.requiredAttributeTypes_.some(
        req => req.typeName === attributeInstance.type.typeName);
    return isRequired ? '*' : '';
  }

  /**
   * Computes the label for cr-input fields.
   * Appends '*' to the label text if required.
   */
  protected computeInputLabel_(attributeInstance: AttributeInstance): string {
    return attributeInstance.type.typeNameAsString +
        this.getRequiredIndicator_(attributeInstance);
  }

  private walletManageYourInfoUrl_(entityType: EntityType): string {
    assert(entityType.passType);
    return entityType.passType ===
            chrome.autofillPrivate.EntityPassType.PUBLIC_PASS ?
        loadTimeData.getString('managePublicPassesUrl') :
        loadTimeData.getString('managePrivatePassesUrl');
  }

  protected shouldHideFooterText_(): boolean {
    return this.footerText_.toString() === '';
  }

  private isPublicPass_(entityType: EntityType): boolean {
    return entityType.supportsWalletStorage &&
        entityType.passType ===
        chrome.autofillPrivate.EntityPassType.PUBLIC_PASS;
  }

  // When saving a Wallet private pass a consent is recorded that includes the
  // notice string. Ensure that the correct string ID is referenced in the
  // backend code.
  // LINT.IfChange
  private computeFooterText_(): TrustedHTML {
    if (!this.entityInstance || !this.userEmail_) {
      return sanitizeInnerHtml('');
    }

    const isPublicPassWithDisclosure =
        loadTimeData.getBoolean('enableWalletDisclosureNoticePublicPass') &&
        this.isPublicPass_(this.entityInstance.type);

    if (!this.entityInstance.type.supportsWalletStorage ||
        this.entityInstance.guid ||
        (isPublicPassWithDisclosure && !this.upsertPassDetails)) {
      return sanitizeInnerHtml(
          this.i18n('autofillAiSaveOrUpdateLocalEntitySourceNotice'));
    }

    const walletTitle = this.i18n('googleWalletTitle');
    let walletNotice: TrustedHTML;
    if (loadTimeData.getBoolean('enableAutofillAiWalletPrivatePasses')) {
      // Show footer only when it is a new entity and type supports Wallet
      // storage. This is sufficient because the entities stored in Wallet are
      // not editable from the settings.
      const manageYourInfoLink = `<a target=_blank href=${
          this.walletManageYourInfoUrl_(this.entityInstance.type)}>${
          this.i18n('autofillAiManageYourInfo')}</a>`;
      walletNotice =
          this.i18nAdvanced('saveInfoToWalletSettingsAccountNotice', {
            substitutions:
                [walletTitle, manageYourInfoLink, walletTitle, this.userEmail_],
            tags: ['a'],
            attrs: ['href', 'target'],
          });
    } else {
      // Show footer only when it is a new entity and type supports Wallet
      // storage. This is sufficient because the entities stored in Wallet are
      // not editable from the settings.
      walletNotice = sanitizeInnerHtml(this.i18n(
          'saveInfoToWalletAccountNotice', walletTitle, this.userEmail_));
    }

    if (isPublicPassWithDisclosure && this.upsertPassDetails) {
      const legalMessage = this.formatLegalMessageLines_(
          this.upsertPassDetails.legalMessageLines);
      if (legalMessage) {
        return sanitizeInnerHtml(
            `<div>${walletNotice.toString()}</div><div>${legalMessage}</div>`);
      }
    }

    return walletNotice;
  }
  // LINT.ThenChange(//chrome/browser/extensions/api/autofill_private/autofill_private_api.cc)

  private formatLegalMessageLines_(
      lines: chrome.autofillPrivate.LegalMessageLine[]): string {
    let html = '';
    for (let i = 0; i < lines.length; ++i) {
      const line = lines[i];
      let lineHtml = '';
      let lastIndex = 0;
      const sortedLinks = [...line.links].sort((a, b) => a.start - b.start);
      for (const link of sortedLinks) {
        lineHtml += line.text.substring(lastIndex, link.start);
        const linkText = line.text.substring(link.start, link.end);
        lineHtml += `<a target="_blank" href="${link.url}">${linkText}</a>`;
        lastIndex = link.end;
      }
      lineHtml += line.text.substring(lastIndex);
      html += (i > 0 ? '<br>' : '') + lineHtml;
    }
    return html;
  }

  protected isExistingYearOutOfBounds_(attributeInstance: AttributeInstance):
      boolean {
    const year = this.getExistingYear_(attributeInstance);
    return year.length > 0 && !this.years_.includes(year);
  }

  protected getExistingYear_(attributeInstance: AttributeInstance): string {
    // Look up the attribute instance in `this.entityInstance` instead of
    // reading `attributeInstance.value` directly, because `attributeInstance`
    // in `this.completeAttributeInstanceList_` is mutated when the user selects
    // another year, and the return value of `getExistingYear_` (and
    // `isExistingYearOutOfBounds_`) should not change when the user selects
    // another year.
    assert(this.entityInstance);
    const existingAttributeInstance =
        this.entityInstance.attributeInstances.find(
            attr => attr.type.typeName === attributeInstance.type.typeName);
    // `existingAttributeInstance` is undefined when adding a new entity or when
    // an existing entity does not have this date attribute populated.
    return existingAttributeInstance ?
        (existingAttributeInstance.value as DateValue).year :
        '';
  }

  /**
   * This function returns a string that can be used in a srcset to scale
   * the provided `url` based on the user's screen resolution.
   */
  protected getScaledSrcSet_(url: string): string {
    return `${url} 1x, ${url}@2x 2x`;
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
  protected isDateInvalid_(attributeInstance: AttributeInstance): boolean {
    if (attributeInstance.type.dataType !==
            chrome.autofillPrivate.AttributeTypeDataType.DATE ||
        !this.userClickedSaveButton_) {
      return false;
    }

    if (this.isFieldInvalid_(attributeInstance)) {
      return true;
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
   * Returns true if the field should be highlighted as invalid due to
   * missing requirements.
   */
  protected isFieldInvalid_(attributeInstance: AttributeInstance): boolean {
    // Don't show errors before the user tries to save.
    if (!this.userClickedSaveButton_ || !this.validationError_) {
      return false;
    }

    // Check if this specific field is one of the required candidates.
    const isRequiredCandidate = this.requiredAttributeTypes_.some(
        req => req.typeName === attributeInstance.type.typeName);

    return isRequiredCandidate &&
        !this.isAttributeInstanceNotEmpty(attributeInstance);
  }

  protected shouldShowWalletBranding_(): boolean {
    if (!this.entityInstance || this.entityInstance.guid) {
      return false;
    }

    if (this.isPublicPass_(this.entityInstance.type)) {
      return loadTimeData.getBoolean('enableWalletDisclosureNoticePublicPass') ?
          !!this.upsertPassDetails :
          this.entityInstance.type.supportsWalletStorage;
    }

    return this.entityInstance.type.supportsWalletStorage;
  }

  /**
   * Returns true if the value is not empty and it is not made out only of
   * whitespaces.
   * For dates, at least one of month, day and year has to be not empty. An
   * incomplete date is not an empty field.
   */
  private isAttributeInstanceNotEmpty(attributeInstance: AttributeInstance):
      boolean {
    if (attributeInstance.type.dataType ===
        chrome.autofillPrivate.AttributeTypeDataType.DATE) {
      const value: DateValue = attributeInstance.value as DateValue;
      return value.month.trim().length > 0 || value.day.trim().length > 0 ||
          value.year.trim().length > 0;
    }
    return (attributeInstance.value as string).trim().length > 0;
  }

  protected onAttributeInstanceFieldInput_(_e: Event): void {
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

    const requiredFieldsMet = this.checkRequiredFields_();

    if (!requiredFieldsMet) {
      const requiredFieldNames =
          this.requiredAttributeTypes_.map(type => type.typeNameAsString);

      const formatter = new Intl.ListFormat(document.documentElement.lang, {
        style: 'long',
        type: 'disjunction',
      });

      const formattedList = formatter.format(requiredFieldNames);
      this.validationError_ = this.i18n(
          'autofillAiAddOrEditDialogRequiredFieldError', formattedList);
    } else if (this.allFieldsAreEmpty_) {
      this.validationError_ =
          this.i18n('autofillAiAddOrEditDialogValidationError');
    } else {
      this.validationError_ = '';
    }

    this.canSave_ =
        !this.allFieldsAreEmpty_ && !invalidDateExists && requiredFieldsMet;
  }

  /**
   * Helper to determine if the spinner should be visible.
   */
  protected shouldShowSpinner_(): boolean {
    return this.enableSavePrivatePassesToWallet_ && this.saveInProgress_;
  }

  protected onCancelClick_(): void {
    if (this.saveInProgress_) {
      // Prevent canceling while a save is in progress to avoid state
      // inconsistencies.
      return;
    }
    this.$.dialog.cancel();
  }

  protected onDialogCancel_(e: Event): void {
    if (this.saveInProgress_) {
      e.preventDefault();
    }
  }

  protected async onConfirmClick_(): Promise<void> {
    if (this.saveInProgress_) {
      return;
    }
    this.userClickedSaveButton_ = true;
    this.validateForm_();
    if (!this.canSave_) {
      return;
    }

    const entityToSave = {...this.entityInstance!};

    entityToSave.attributeInstances =
        this.completeAttributeInstanceList_.filter(
            attributeInstance =>
                this.isAttributeInstanceNotEmpty(attributeInstance));

    // If the type supports Wallet storage, we default to saving to Wallet but
    // only for new entities.
    if (!entityToSave.guid) {
      if (this.isPublicPass_(entityToSave.type) &&
          loadTimeData.getBoolean('enableWalletDisclosureNoticePublicPass')) {
        entityToSave.storedInWallet = !!this.upsertPassDetails;
        entityToSave.contextToken = this.upsertPassDetails?.contextToken;
      } else {
        entityToSave.storedInWallet = entityToSave.type.supportsWalletStorage;
      }
    }

    if (this.enableSavePrivatePassesToWallet_) {
      this.saveInProgress_ = true;
      getAnnouncerInstance().announce(
          this.i18n('saveToWalletLoadingStateA11y'));

      try {
        await this.entityDataManager_.addOrUpdateEntityInstance(entityToSave);
        this.saveInProgress_ = false;
      } catch (e) {
        this.saveInProgress_ = false;
        return;
      }
    }

    this.fire('autofill-ai-add-or-edit-done', entityToSave);

    this.$.dialog.close();
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
