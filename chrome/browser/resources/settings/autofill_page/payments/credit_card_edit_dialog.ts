// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-credit-card-edit-dialog' is the dialog that allows
 * editing or creating a credit card entry.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';

import {getCss} from './credit_card_edit_dialog.css.js';
import {getHtml} from './credit_card_edit_dialog.html.js';

/**
 * Regular expression for invalid nickname. Nickname containing any digits will
 * be treated as invalid.
 */
const NICKNAME_INVALID_REGEX: RegExp = new RegExp('.*\\d+.*');

/**
 * Enum of possible states for the credit card number. A card number is valid
 * if it is of a supported length and passes a Luhn check. Otherwise, it is
 * invalid and we may show an error to the user in cases where we are certain
 * they have entered an invalid card (i.e. vs still typing).
 */
enum CardNumberValidationState {
  VALID = 'valid',
  INVALID_NO_ERROR = 'invalid-no-error',
  INVALID_WITH_ERROR = 'invalid-with-error',
}

declare global {
  interface HTMLElementEventMap {
    'save-credit-card': CustomEvent<chrome.autofillPrivate.CreditCardEntry>;
  }
}

export interface SettingsCreditCardEditDialogElement {
  $: {
    cancelButton: CrButtonElement,
    cvcInput: CrInputElement,
    dialog: CrDialogElement,
    expiredError: HTMLElement,
    month: HTMLSelectElement,
    nameInput: CrInputElement,
    nicknameInput: CrInputElement,
    numberInput: CrInputElement,
    saveButton: CrButtonElement,
    year: HTMLSelectElement,
  };
}

const SettingsCreditCardEditDialogElementBase =
    PrefServiceObserverMixinLit(I18nMixinLit(CrLitElement));

export class SettingsCreditCardEditDialogElement extends
    SettingsCreditCardEditDialogElementBase {
  static get is() {
    return 'settings-credit-card-edit-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      cvcStoragePref_: {type: Object},

      /**
       * The underlying credit card object for the dialog. After initialization
       * of the dialog, this object is only modified once the 'Save' button is
       * clicked.
       */
      creditCard: {type: Object},

      /**
       * The actual title that's used for this dialog. Will be context sensitive
       * based on if |creditCard| is being created or edited.
       */
      title_: {type: String},

      /**
       * The list of months to show in the dropdown.
       */
      monthList_: {type: Array},

      /** The list of years to show in the dropdown. */
      yearList_: {type: Array},

      /**
       * Backing data for inputs in the dialog, each bound to the corresponding
       * HTML elements.
       *
       * Note that rawCardNumber_ is unsanitized; code should instead use
       * `sanitizedCardNumber_`.
       */
      name_: {type: String},
      rawCardNumber_: {type: String},
      cvc_: {type: String},
      nickname_: {type: String},
      expirationYear_: {type: String},
      expirationMonth_: {type: String},

      /**
       * A sanitized version of `rawCardNumber_` that strips out commonly used
       * separators and trims whitespace.
       */
      sanitizedCardNumber_: {type: String},

      /** Whether the current nickname input is invalid. */
      nicknameInvalid_: {type: Boolean},

      /** Whether the current card number field is invalid. */
      cardNumberValidationState_: {type: String},

      /**
       * Computed property that tracks if the entered credit card is expired -
       * that is, if its expiration month and year are in the past.
       */
      expired_: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Checks if CVC storage is available based on the feature flag.
       */
      cvcStorageAvailable_: {type: Boolean},
    };
  }

  private accessor cvcStoragePref_: chrome.settingsPrivate.PrefObject<boolean>|
      undefined;
  accessor creditCard: chrome.autofillPrivate.CreditCardEntry = {
    expirationMonth: '01',
    expirationYear: '2099',
  };
  protected accessor title_: string = '';
  protected accessor monthList_: string[] = [
    '01',
    '02',
    '03',
    '04',
    '05',
    '06',
    '07',
    '08',
    '09',
    '10',
    '11',
    '12',
  ];
  protected accessor yearList_: string[] = [];
  protected accessor name_: string = '';
  protected accessor rawCardNumber_: string = '';
  protected accessor cvc_: string = '';
  protected accessor nickname_: string = '';
  protected accessor expirationYear_: string = '';
  protected accessor expirationMonth_: string = '';
  private accessor sanitizedCardNumber_: string = '';
  protected accessor nicknameInvalid_: boolean = false;
  private accessor cardNumberValidationState_: CardNumberValidationState =
      CardNumberValidationState.INVALID_NO_ERROR;
  private accessor expired_: boolean = false;
  private accessor cvcStorageAvailable_: boolean =
      loadTimeData.getBoolean('cvcStorageAvailable');

  override connectedCallback() {
    this.mirrorPref('autofill.payment_cvc_storage', 'cvcStoragePref_');

    this.title_ = this.i18n(
        this.creditCard.guid ? 'editCreditCardTitle' : 'addCreditCardTitle');

    // Add a leading '0' if a month is 1 char.
    if (this.creditCard.expirationMonth!.length === 1) {
      this.creditCard.expirationMonth = '0' + this.creditCard.expirationMonth;
    }

    const date = new Date();
    let firstYear = date.getFullYear();
    let lastYear = firstYear + 19;  // Show next 19 years (20 total).
    let selectedYear = parseInt(this.creditCard.expirationYear!, 10);

    // |selectedYear| must be valid and between first and last years.
    if (!selectedYear) {
      selectedYear = firstYear;
    } else if (selectedYear < firstYear) {
      firstYear = selectedYear;
    } else if (selectedYear > lastYear) {
      lastYear = selectedYear;
    }

    const yearList = [];
    for (let i = firstYear; i <= lastYear; ++i) {
      yearList.push(i.toString());
    }
    this.yearList_ = yearList;

    super.connectedCallback();

    this.expirationYear_ = selectedYear.toString();
    this.expirationMonth_ = this.creditCard.expirationMonth || '';
    this.cvc_ = this.creditCard.cvc || '';
    this.name_ = this.creditCard.name || '';
    this.rawCardNumber_ = this.creditCard.cardNumber || '';
    this.nickname_ = this.creditCard.nickname || '';
    this.$.dialog.showModal();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('rawCardNumber_')) {
      this.sanitizedCardNumber_ = this.sanitizeCardNumber_();
    }
    if (changedPrivateProperties.has('sanitizedCardNumber_')) {
      this.onSanitizedCardNumberChanged_();
    }
    if (changedPrivateProperties.has('expirationMonth_') ||
        changedPrivateProperties.has('expirationYear_')) {
      this.expired_ = this.computeExpired_();
    }
    if (changedPrivateProperties.has('nickname_')) {
      // Validate no digits are used in nickname. Display error message and
      // disable the save button when invalid.
      this.nicknameInvalid_ = NICKNAME_INVALID_REGEX.test(this.nickname_);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('expired_')) {
      this.onExpiredChanged_();
    }
    if (changedPrivateProperties.has('monthList_')) {
      this.$.month.value = this.expirationMonth_;
    }
    if (changedPrivateProperties.has('yearList_')) {
      this.$.year.value = this.expirationYear_;
    }
  }

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  }

  protected onRawCardNumberValueChanged_(e: CustomEvent<{value: string}>) {
    this.rawCardNumber_ = e.detail.value;
  }

  protected onCvcValueChanged_(e: CustomEvent<{value: string}>) {
    this.cvc_ = e.detail.value;
  }

  protected onNameValueChanged_(e: CustomEvent<{value: string}>) {
    this.name_ = e.detail.value;
  }

  protected onNicknameValueChanged_(e: CustomEvent<{value: string}>) {
    this.nickname_ = e.detail.value;
  }

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   */
  protected onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  /**
   * Handler for tapping the save button.
   */
  protected onSaveButtonClick_() {
    if (!this.saveEnabled_()) {
      return;
    }

    this.creditCard.expirationYear = this.expirationYear_;
    this.creditCard.expirationMonth = this.expirationMonth_;
    this.creditCard.name = this.name_;
    this.creditCard.cardNumber = this.sanitizedCardNumber_;
    this.creditCard.nickname = this.nickname_;
    // Take the user entered CVC input as-is. This is due to PCI compliance.
    this.creditCard.cvc = this.cvc_;
    this.trimCreditCard_();
    this.fire('save-credit-card', this.creditCard);
    this.close();
  }

  private onSanitizedCardNumberChanged_() {
    this.cardNumberValidationState_ = this.computeCardNumberValidationState_(
        this.sanitizedCardNumber_, /*isBlur=*/ false);
  }

  protected onNumberInputBlur_(event: Event) {
    assert(event.type === 'blur');
    this.cardNumberValidationState_ = this.computeCardNumberValidationState_(
        this.sanitizedCardNumber_, /*isBlur=*/ true);
  }

  protected showErrorForCardNumber_(): boolean {
    return this.cardNumberValidationState_ ===
        CardNumberValidationState.INVALID_WITH_ERROR;
  }

  protected onMonthChange_() {
    this.expirationMonth_ = this.monthList_[this.$.month.selectedIndex] || '';
  }

  protected onYearChange_() {
    this.expirationYear_ = this.yearList_[this.$.year.selectedIndex] || '';
  }

  /**
   * Handles a11y error announcement the same way as in cr-input.
   */
  private onExpiredChanged_() {
    const errorElement = this.$.expiredError;
    const ERROR_ID = errorElement.id;
    // Readding attributes is needed for consistent announcement by VoiceOver
    if (this.expired_) {
      errorElement.setAttribute('role', 'alert');
      this.$.month.setAttribute('aria-errormessage', ERROR_ID);
      this.$.year.setAttribute('aria-errormessage', ERROR_ID);
    } else {
      errorElement.removeAttribute('role');
      this.$.month.removeAttribute('aria-errormessage');
      this.$.year.removeAttribute('aria-errormessage');
    }
  }

  /**
   * @return 'true' or 'false' for the aria-invalid attribute
   *     of expiration selectors.
   */
  protected getExpirationAriaInvalid_(): string {
    return this.expired_ ? 'true' : 'false';
  }

  protected checkIfCvcStorageIsAvailable_(): boolean {
    return this.cvcStorageAvailable_ && !!this.cvcStoragePref_?.value;
  }

  protected getCvcImageSource_(): string {
    // An icon is shown to the user to help them look for their CVC.
    // The location differs for AmEx and non-AmEx cards, so we have to get
    // the first two digits of the card number for AmEx cards before we can
    // update the icon.
    return this.isCardAmex_() ? 'chrome://settings/images/cvc_amex.svg' :
                                'chrome://settings/images/cvc.svg';
  }

  protected getCvcImageTooltip_(): string {
    // An icon is shown to the user to help them look for their CVC.
    // The location differs for AmEx and non-AmEx cards, so we have to get
    // the first two digits of the card number for AmEx cards before we can
    // update the icon.
    return this.i18n(
        this.isCardAmex_() ? 'creditCardCvcAmexImageTitle' :
                             'creditCardCvcImageTitle');
  }

  protected saveEnabled_(): boolean {
    if (this.cardNumberValidationState_ !== CardNumberValidationState.VALID) {
      return false;
    }

    return !this.expired_ && !this.nicknameInvalid_;
  }

  /**
   * @return True iff the provided expiration date is passed.
   */
  private computeExpired_(): boolean {
    if (!this.expirationYear_ || !this.expirationMonth_) {
      return false;
    }
    const now = new Date();
    // Convert string (e.g. '06') to number (e.g. 6) for comparison.
    const expirationYear = parseInt(this.expirationYear_, 10);
    const expirationMonth = parseInt(this.expirationMonth_, 10);
    return (
        expirationYear < now.getFullYear() ||
        (expirationYear === now.getFullYear() &&
         expirationMonth <= now.getMonth()));
  }

  /**
   * Trim credit card's name, cardNumber and nickname if exist.
   */
  private trimCreditCard_() {
    if (this.creditCard.name) {
      this.creditCard.name = this.creditCard.name.trim();
    }
    if (this.creditCard.cardNumber) {
      this.creditCard.cardNumber = this.creditCard.cardNumber.trim();
    }
    if (this.creditCard.nickname) {
      this.creditCard.nickname = this.creditCard.nickname.trim();
    }
  }

  private isCardAmex_(): boolean {
    const cardNumber = this.sanitizedCardNumber_;
    return !!cardNumber && cardNumber.length >= 2 &&
        !!cardNumber.match('^(34|37)');
  }

  /**
   * Sanitize the raw card number entered by the user, trimming whitespace and
   * removing commonly used separators.
   */
  private sanitizeCardNumber_(): string {
    return this.rawCardNumber_ ?
        this.rawCardNumber_.trim().replaceAll(/ |-/g, '') :
        '';
  }

  /**
   * Compute whether or not the provided card number is valid, i.e. that it is a
   * number and passes a Luhn check. If the card number isn't complete yet, it
   * is still considered invalid but no error will be shown.
   */
  private computeCardNumberValidationState_(
      sanitizedCardNumber: string,
      isBlur: boolean = false): CardNumberValidationState {
    // The card number field must only contain digits.
    if (/[^\d]/.test(sanitizedCardNumber)) {
      return CardNumberValidationState.INVALID_WITH_ERROR;
    }

    // A credit card number is only valid if it passes a Luhn check. We do not
    // want to show an 'invalid card' error to users if they have not yet
    // finished typing the card number, but unfortunately different credit cards
    // can have different card number lengths.
    //
    // In order to minimize false-positive errors, we implement the following
    // algorithm:
    //
    //   1. If the user enters < 12 digits (the minimum supported card
    //      number length) then no error will be shown but the Save button will
    //      not be enabled.
    //   2. If the user enters < 16 digits (the most common card number
    //      length) and the number fails a Luhn check, then no error will be
    //      shown but the Save button will not be enabled.
    //   3. If the user enters >= 16 digits and the number fails a Luhn check,
    //      then an error will be shown and the Save button will not be enabled.
    //   4. If the user enters > 19 digits (the maximum supported card number
    //      length) then an error will be shown and the Save button will not be
    //      enabled.
    //   5. If the user changes focus to another field and the number of digits
    //      is outside the allowed lengths or the card number fails a Luhn
    //      check, then an error will be shown and the Save button will not be
    //      enabled.
    //
    // The cases are handled in reverse for simplicity of code.

    // Case (5) - the user has switched focus to another element.
    if (isBlur) {
      return (sanitizedCardNumber.length >= 12 &&
              sanitizedCardNumber.length <= 19 &&
              this.passesLuhnCheck_(sanitizedCardNumber)) ?
          CardNumberValidationState.VALID :
          CardNumberValidationState.INVALID_WITH_ERROR;
    }

    // Case (4) - the user entered a card number that is too long.
    if (sanitizedCardNumber.length > 19) {
      return CardNumberValidationState.INVALID_WITH_ERROR;
    }

    // Case (3) - the user has entered at least 16 digits.
    if (sanitizedCardNumber.length >= 16) {
      return this.passesLuhnCheck_(sanitizedCardNumber) ?
          CardNumberValidationState.VALID :
          CardNumberValidationState.INVALID_WITH_ERROR;
    }

    // Case (2) - the user has entered at least 12 digits.
    if (sanitizedCardNumber.length >= 12) {
      return this.passesLuhnCheck_(sanitizedCardNumber) ?
          CardNumberValidationState.VALID :
          CardNumberValidationState.INVALID_NO_ERROR;
    }

    // Case (1) - the user has entered less than 12 digits.
    return CardNumberValidationState.INVALID_NO_ERROR;
  }

  /**
   * Validates if a given card number passes a Luhn check.
   *
   * http://en.wikipedia.org/wiki/Luhn_algorithm
   */
  private passesLuhnCheck_(cardNumber: string): boolean {
    let sum = 0;
    let odd = false;
    const cardNumberDigits = cardNumber.split('').reverse();
    for (const digit of cardNumberDigits) {
      let intDigit = Number(digit);
      if (Number.isNaN(intDigit)) {
        return false;
      }

      if (odd) {
        intDigit *= 2;
        sum += Math.floor(intDigit / 10) + (intDigit % 10);
      } else {
        sum += intDigit;
      }
      odd = !odd;
    }

    return (sum % 10) === 0;
  }
}

export type CreditCardEditDialogElement = SettingsCreditCardEditDialogElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-credit-card-edit-dialog': SettingsCreditCardEditDialogElement;
  }
}

customElements.define(
    SettingsCreditCardEditDialogElement.is,
    SettingsCreditCardEditDialogElement);
