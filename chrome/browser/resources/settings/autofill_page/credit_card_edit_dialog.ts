// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-credit-card-edit-dialog' is the dialog that allows
 * editing or creating a credit card entry.
 */

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './credit_card_edit_dialog.html.js';

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

const SettingsCreditCardEditDialogElementBase = I18nMixin(PolymerElement);

export class SettingsCreditCardEditDialogElement extends
    SettingsCreditCardEditDialogElementBase {
  static get is() {
    return 'settings-credit-card-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * User preferences state.
       */
      prefs: Object,

      /**
       * The underlying credit card object for the dialog. After initialization
       * of the dialog, this object is only modified once the 'Save' button is
       * clicked.
       */
      creditCard: Object,

      /**
       * The actual title that's used for this dialog. Will be context sensitive
       * based on if |creditCard| is being created or edited.
       */
      title_: String,

      /**
       * The list of months to show in the dropdown.
       */
      monthList_: {
        type: Array,
        value: [
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
        ],
      },

      /** The list of years to show in the dropdown. */
      yearList_: Array,

      /**
       * Backing data for inputs in the dialog, each bound to the corresponding
       * HTML elements.
       *
       * Note that rawCardNumber_ is unsanitized; code should instead use
       * `sanitizedCardNumber_`.
       */
      name_: String,
      rawCardNumber_: String,
      cvc_: String,
      nickname_: String,
      expirationYear_: String,
      expirationMonth_: String,

      /**
       * A sanitized version of `rawCardNumber_` that strips out commonly used
       * separators and trims whitespace.
       */
      sanitizedCardNumber_: {
        type: String,
        computed: 'sanitizeCardNumber_(rawCardNumber_)',
        observer: 'onSanitizedCardNumberChanged_',
      },

      /** Whether the current nickname input is invalid. */
      nicknameInvalid_: {
        type: Boolean,
        value: false,
      },

      /** Whether the current card number field is invalid. */
      cardNumberValidationState_: {
        type: CardNumberValidationState,
        value: false,
      },

      /**
       * Computed property that tracks if the entered credit card is expired -
       * that is, if its expiration month and year are in the past.
       */
      expired_: {
        type: Boolean,
        computed: 'computeExpired_(expirationMonth_, expirationYear_)',
        reflectToAttribute: true,
        observer: 'onExpiredChanged_',
      },

      /**
       * Checks if CVC storage is available based on the feature flag.
       */
      cvcStorageAvailable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('cvcStorageAvailable');
        },
      },

      /**
       * Checks if card numbers must be validated based on the feature flag.
       */
      requireValidLocalCardsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('requireValidLocalCards');
        },
      },
    };
  }

  prefs: {[key: string]: any};
  creditCard: chrome.autofillPrivate.CreditCardEntry;
  private title_: string;
  private monthList_: string[];
  private yearList_: string[];
  private name_?: string;
  private rawCardNumber_: string;
  private cvc_?: string;
  private nickname_?: string;
  private expirationYear_?: string;
  private expirationMonth_?: string;
  private sanitizedCardNumber_: string;
  private nicknameInvalid_: boolean;
  private cardNumberValidationState_: CardNumberValidationState;
  private expired_: boolean;
  private cvcStorageAvailable_: boolean;
  private requireValidLocalCardsEnabled_: boolean;

  override connectedCallback() {
    super.connectedCallback();

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

    microTask.run(() => {
      this.expirationYear_ = selectedYear.toString();
      this.expirationMonth_ = this.creditCard.expirationMonth;
      this.cvc_ = this.creditCard.cvc;
      this.name_ = this.creditCard.name;
      this.rawCardNumber_ = this.creditCard.cardNumber || '';
      this.nickname_ = this.creditCard.nickname;
      this.$.dialog.showModal();
    });
  }

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  }

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   */
  private onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  /**
   * Handler for tapping the save button.
   */
  private onSaveButtonClick_() {
    if (!this.saveEnabled_()) {
      return;
    }

    this.creditCard.expirationYear = this.expirationYear_;
    this.creditCard.expirationMonth = this.expirationMonth_;
    this.creditCard.name = this.name_;
    if (this.requireValidLocalCardsEnabled_) {
      this.creditCard.cardNumber = this.sanitizedCardNumber_;
    } else {
      // To preserve legacy behavior, save the raw card number directly.
      this.creditCard.cardNumber = this.rawCardNumber_;
    }
    this.creditCard.nickname = this.nickname_;
    // Take the user entered CVC input as-is. This is due to PCI compliance.
    this.creditCard.cvc = this.cvc_;
    this.trimCreditCard_();
    this.dispatchEvent(new CustomEvent(
        'save-credit-card',
        {bubbles: true, composed: true, detail: this.creditCard}));
    this.close();
  }

  private onSanitizedCardNumberChanged_() {
    this.cardNumberValidationState_ = this.computeCardNumberValidationState_(
        this.sanitizedCardNumber_, /*isBlur=*/ false);
  }

  private onNumberInputBlurred_(event: Event) {
    assert(event.type === 'blur');
    this.cardNumberValidationState_ = this.computeCardNumberValidationState_(
        this.sanitizedCardNumber_, /*isBlur=*/ true);
  }

  private showErrorForCardNumber_(cardNumberValidationState:
                                      CardNumberValidationState) {
    return cardNumberValidationState ===
        CardNumberValidationState.INVALID_WITH_ERROR;
  }

  private onMonthChange_() {
    this.expirationMonth_ = this.monthList_[this.$.month.selectedIndex];
  }

  private onYearChange_() {
    this.expirationYear_ = this.yearList_[this.$.year.selectedIndex];
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
      this.shadowRoot!.querySelector(`#month`)!.setAttribute(
          'aria-errormessage', ERROR_ID);
      this.shadowRoot!.querySelector(`#year`)!.setAttribute(
          'aria-errormessage', ERROR_ID);
    } else {
      errorElement.removeAttribute('role');
      this.shadowRoot!.querySelector(`#month`)!.removeAttribute(
          'aria-errormessage');
      this.shadowRoot!.querySelector(`#year`)!.removeAttribute(
          'aria-errormessage');
    }
  }

  /**
   * @return 'true' or 'false' for the aria-invalid attribute
   *     of expiration selectors.
   */
  private getExpirationAriaInvalid_(): string {
    return this.expired_ ? 'true' : 'false';
  }

  private checkIfCvcStorageIsAvailable_(cvcStorageToggleEnabled: boolean):
      boolean {
    return this.cvcStorageAvailable_ && cvcStorageToggleEnabled;
  }

  private getCvcImageSource_(): string {
    // An icon is shown to the user to help them look for their CVC.
    // The location differs for AmEx and non-AmEx cards, so we have to get
    // the first two digits of the card number for AmEx cards before we can
    // update the icon.
    return this.isCardAmex_() ? 'chrome://settings/images/cvc_amex.svg' :
                                'chrome://settings/images/cvc.svg';
  }

  private getCvcImageTooltip_(): string {
    // An icon is shown to the user to help them look for their CVC.
    // The location differs for AmEx and non-AmEx cards, so we have to get
    // the first two digits of the card number for AmEx cards before we can
    // update the icon.
    return this.i18n(
        this.isCardAmex_() ? 'creditCardCvcAmexImageTitle' :
                             'creditCardCvcImageTitle');
  }

  /**
   * Validate no digits are used in nickname. Display error message and disable
   * the save button when invalid.
   */
  private validateNickname_() {
    this.nicknameInvalid_ = NICKNAME_INVALID_REGEX.test(this.nickname_!);
  }

  /**
   * @param  nickname of the card, undefined when not set.
   * @return nickname character length.
   */
  private computeNicknameCharCount_(nickname?: string): number {
    return (nickname || '').length;
  }

  private saveEnabled_() {
    if (this.requireValidLocalCardsEnabled_ &&
        this.cardNumberValidationState_ !== CardNumberValidationState.VALID) {
      return false;
    }

    // Either the card name or card number must be non-empty to save.
    //
    // TODO(crbug.com/40285360): Once `this.requireValidLocalCardsEnabled_` is
    // enabled, this block can be removed, as this.rawCardNumber_ will always be
    // non-empty if we pass the above check.
    const nameMissing = !this.name_ || !this.name_.trim();
    // To preserve legacy behavior, check the raw card directly.
    const cardNumberMissing =
        !this.rawCardNumber_ || !this.rawCardNumber_.trim();
    if (nameMissing && cardNumberMissing) {
      return false;
    }

    if (this.expired_) {
      return false;
    }

    if (this.nicknameInvalid_) {
      return false;
    }

    return true;
  }

  /**
   * @return True iff the provided expiration date is passed.
   */
  private computeExpired_(): boolean {
    if (this.expirationYear_ === undefined ||
        this.expirationMonth_ === undefined) {
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
    const cardNumber = this.requireValidLocalCardsEnabled_ ?
        this.sanitizedCardNumber_ :
        this.rawCardNumber_;
    return !!cardNumber && cardNumber.length >= 2 &&
        !!cardNumber.match('^(34|37)');
  }

  /**
   * Sanitize the raw card number entered by the user, trimming whitespace and
   * removing commonly used separators.
   */
  private sanitizeCardNumber_(cardNumber: string): string {
    return cardNumber ? cardNumber.trim().replaceAll(/ |-/g, '') : '';
  }

  /**
   * Compute whether or not the provided card number is valid, i.e. that it is a
   * number and passes a Luhn check. If the card number isn't complete yet, it
   * is still considered invalid but no error will be shown.
   */
  private computeCardNumberValidationState_(
      sanitizedCardNumber: string,
      isBlur: boolean = false): CardNumberValidationState {
    if (!this.requireValidLocalCardsEnabled_) {
      return CardNumberValidationState.VALID;
    }

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

declare global {
  interface HTMLElementTagNameMap {
    'settings-credit-card-edit-dialog': SettingsCreditCardEditDialogElement;
  }
}

customElements.define(
    SettingsCreditCardEditDialogElement.is,
    SettingsCreditCardEditDialogElement);
