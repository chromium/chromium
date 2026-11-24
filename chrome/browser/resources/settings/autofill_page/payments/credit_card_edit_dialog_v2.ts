// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-credit-card-edit-dialog-v2' is the dialog that allows
 * editing or creating a credit card entry.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {ProfileInfo} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import {ProfileInfoBrowserProxyImpl} from '/shared/settings/people_page/profile_info_browser_proxy.js';
// <if expr="not is_chromeos">
import type {StoredAccount} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
// </if>
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';
// <if expr="is_chromeos">
import {AccountManagerBrowserProxyImpl} from '../../people_page/account_manager_browser_proxy.js';

// </if>

import {getCss} from './credit_card_edit_dialog_v2.css.js';
import {getHtml} from './credit_card_edit_dialog_v2.html.js';

/**
 * Regular expression for invalid nickname. Nickname containing any digits will
 * be treated as invalid.
 */
const NICKNAME_INVALID_REGEX: RegExp = new RegExp('.*\\d+.*');

/**
 * Enum of possible states for the credit card number and expiration date. A
 * card number is valid if it is of a supported length and passes a Luhn
 * check. An expiration date is valid if it is a complete MM/YY date with a
 * valid month (01-12) that is not in the past. Otherwise, the input is invalid
 * and we may show an error to the user in cases where we are certain they have
 * entered an invalid value (i.e. vs still typing).
 */
enum ValidationState {
  VALID = 'valid',
  INVALID_NO_ERROR = 'invalid-no-error',
  INVALID_WITH_ERROR = 'invalid-with-error',
}

declare global {
  interface HTMLElementEventMap {
    'save-credit-card': CustomEvent<chrome.autofillPrivate.CreditCardEntry>;
  }
}

export interface SettingsCreditCardEditDialogV2Element {
  $: {
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    expirationDateInput: CrInputElement,
    nameInput: CrInputElement,
    nicknameInput: CrInputElement,
    numberInput: CrInputElement,
    saveButton: CrButtonElement,
  };
}

const SettingsCreditCardEditDialogV2ElementBase = WebUiListenerMixinLit(
    PrefServiceObserverMixinLit(I18nMixinLit(CrLitElement)));

export class SettingsCreditCardEditDialogV2Element extends
    SettingsCreditCardEditDialogV2ElementBase {
  static get is() {
    return 'settings-credit-card-edit-dialog-v2';
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
      expirationDate_: {type: String},
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

      /** Whether the current expiration date field is invalid. */
      expirationDateValidationState_: {type: String},

      primaryAccountName_: {type: String},
      primaryAccountEmail_: {type: String},
      primaryAccountIconUrl_: {type: String},
    };
  }

  private accessor cvcStoragePref_: chrome.settingsPrivate.PrefObject<boolean>|
      undefined;
  accessor creditCard: chrome.autofillPrivate.CreditCardEntry = {
    expirationMonth: '01',
    expirationYear: '2099',
  };
  protected accessor title_: string = '';
  protected accessor name_: string = '';
  protected accessor rawCardNumber_: string = '';
  protected accessor cvc_: string = '';
  protected accessor nickname_: string = '';
  protected accessor expirationDate_: string = '';
  private accessor expirationYear_: string = '';
  private accessor expirationMonth_: string = '';
  private accessor sanitizedCardNumber_: string = '';
  protected accessor nicknameInvalid_: boolean = false;
  private accessor cardNumberValidationState_: ValidationState =
      ValidationState.INVALID_NO_ERROR;
  private accessor expirationDateValidationState_: ValidationState =
      ValidationState.INVALID_NO_ERROR;
  /**
   * Checks if CVC storage is available based on the feature flag.
   */
  private readonly cvcStorageAvailable_: boolean =
      loadTimeData.getBoolean('cvcStorageAvailable');
  protected accessor primaryAccountName_: string = '';
  protected accessor primaryAccountEmail_: string = '';
  protected accessor primaryAccountIconUrl_: string = '';
  // <if expr="_google_chrome">
  private readonly autofillEnableGradientGoogleLogosEnabled_: boolean =
      loadTimeData.getBoolean('autofillEnableGradientGoogleLogos');
  // </if>

  override connectedCallback() {
    this.mirrorPref('autofill.payment_cvc_storage', 'cvcStoragePref_');

    this.title_ = this.i18n(
        this.creditCard.guid ? 'editCreditCardTitle' : 'addCreditCardTitle');

    // Add a leading '0' if a month is 1 char.
    if (this.creditCard.expirationMonth &&
        this.creditCard.expirationMonth.length === 1) {
      this.creditCard.expirationMonth = '0' + this.creditCard.expirationMonth;
    }

    super.connectedCallback();

    // Populate the initial profile and account info shown in the dialog footer,
    // and update it in place if the profile or account info changes while open.
    ProfileInfoBrowserProxyImpl.getInstance().getProfileInfo().then(
        this.handleProfileInfo_.bind(this));
    this.addWebUiListener(
        'profile-info-changed', this.handleProfileInfo_.bind(this));

    // <if expr="is_chromeos">
    if (loadTimeData.getBoolean('isAccountManagerEnabled')) {
      this.addWebUiListener(
          'accounts-changed', this.updateAccounts_.bind(this));
      this.updateAccounts_();
    }
    // </if>

    // <if expr="not is_chromeos">
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));
    this.addWebUiListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));
    // </if>

    if (this.creditCard.guid && this.creditCard.expirationMonth &&
        this.creditCard.expirationYear) {
      this.expirationMonth_ = this.creditCard.expirationMonth;
      this.expirationYear_ = this.creditCard.expirationYear;
      this.expirationDate_ =
          `${this.expirationMonth_}/${this.expirationYear_.slice(-2)}`;
    } else {
      this.expirationMonth_ = '';
      this.expirationYear_ = '';
      this.expirationDate_ = '';
    }
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
    if (changedPrivateProperties.has('expirationDate_')) {
      this.updateExpirationDateValidationState_(/*isBlur=*/ false);
    }
    if (changedPrivateProperties.has('nickname_')) {
      // Validate no digits are used in nickname. Display error message and
      // disable the save button when invalid.
      this.nicknameInvalid_ = NICKNAME_INVALID_REGEX.test(this.nickname_);
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
        ValidationState.INVALID_WITH_ERROR;
  }

  protected onExpirationDateValueChanged_(e: CustomEvent<{value: string}>) {
    const rawValue = e.detail.value;
    const previousValue = this.expirationDate_;
    const digits = rawValue.replace(/\D/g, '').slice(0, 4);

    let formatted = digits;
    if (digits.length > 2) {
      formatted = `${digits.slice(0, 2)}/${digits.slice(2)}`;
    } else if (
        digits.length === 2 &&
        (rawValue.endsWith('/') || previousValue.length < 2)) {
      formatted = `${digits}/`;
    }

    this.expirationDate_ = formatted;
    if (rawValue !== formatted) {
      this.$.expirationDateInput.value = formatted;
    }
  }

  protected onExpirationDateInputBlur_(event: Event) {
    assert(event.type === 'blur');
    this.updateExpirationDateValidationState_(/*isBlur=*/ true);
  }

  protected showErrorForExpirationDate_(): boolean {
    return this.expirationDateValidationState_ ===
        ValidationState.INVALID_WITH_ERROR;
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
    if (this.cardNumberValidationState_ !== ValidationState.VALID) {
      return false;
    }

    return this.expirationDateValidationState_ === ValidationState.VALID &&
        !this.nicknameInvalid_;
  }

  private updateExpirationDateValidationState_(isBlur: boolean) {
    const digits = this.expirationDate_.replace(/\D/g, '');

    if (digits.length >= 2) {
      const month = parseInt(digits.slice(0, 2), 10);
      if (month < 1 || month > 12) {
        this.expirationMonth_ = '';
        this.expirationYear_ = '';
        this.expirationDateValidationState_ =
            ValidationState.INVALID_WITH_ERROR;
        return;
      }
    }

    if (digits.length < 4) {
      this.expirationMonth_ = '';
      this.expirationYear_ = '';
      this.expirationDateValidationState_ = isBlur ?
          ValidationState.INVALID_WITH_ERROR :
          ValidationState.INVALID_NO_ERROR;
      return;
    }

    const monthStr = digits.slice(0, 2);
    const yearShort = parseInt(digits.slice(2, 4), 10);
    const now = new Date();
    const currentCentury = Math.floor(now.getFullYear() / 100) * 100;
    const fullYear = currentCentury + yearShort;
    const month = parseInt(monthStr, 10);

    const isExpired = fullYear < now.getFullYear() ||
        (fullYear === now.getFullYear() && month <= now.getMonth());

    if (isExpired) {
      this.expirationMonth_ = '';
      this.expirationYear_ = '';
      this.expirationDateValidationState_ = ValidationState.INVALID_WITH_ERROR;
      return;
    }

    this.expirationMonth_ = monthStr;
    this.expirationYear_ = fullYear.toString();
    this.expirationDateValidationState_ = ValidationState.VALID;
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
      sanitizedCardNumber: string, isBlur: boolean = false): ValidationState {
    // The card number field must only contain digits.
    if (/[^\d]/.test(sanitizedCardNumber)) {
      return ValidationState.INVALID_WITH_ERROR;
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
          ValidationState.VALID :
          ValidationState.INVALID_WITH_ERROR;
    }

    // Case (4) - the user entered a card number that is too long.
    if (sanitizedCardNumber.length > 19) {
      return ValidationState.INVALID_WITH_ERROR;
    }

    // Case (3) - the user has entered at least 16 digits.
    if (sanitizedCardNumber.length >= 16) {
      return this.passesLuhnCheck_(sanitizedCardNumber) ?
          ValidationState.VALID :
          ValidationState.INVALID_WITH_ERROR;
    }

    // Case (2) - the user has entered at least 12 digits.
    if (sanitizedCardNumber.length >= 12) {
      return this.passesLuhnCheck_(sanitizedCardNumber) ?
          ValidationState.VALID :
          ValidationState.INVALID_NO_ERROR;
    }

    // Case (1) - the user has entered less than 12 digits.
    return ValidationState.INVALID_NO_ERROR;
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

  private handleProfileInfo_(info: ProfileInfo) {
    if (!this.primaryAccountEmail_) {
      this.primaryAccountName_ = info.name;
      this.primaryAccountIconUrl_ = info.iconUrl;
    }
  }

  // <if expr="is_chromeos">
  private async updateAccounts_() {
    const accounts =
        await AccountManagerBrowserProxyImpl.getInstance().getAccounts();
    if (accounts.length === 0) {
      this.primaryAccountEmail_ = '';
      return;
    }
    this.primaryAccountName_ = accounts[0].fullName;
    this.primaryAccountEmail_ = accounts[0].email;
    this.primaryAccountIconUrl_ = accounts[0].pic;
  }
  // </if>

  // <if expr="not is_chromeos">
  private handleStoredAccounts_(accounts: StoredAccount[]) {
    if (accounts.length === 0) {
      this.primaryAccountEmail_ = '';
      return;
    }
    this.primaryAccountName_ = accounts[0].fullName || this.primaryAccountName_;
    this.primaryAccountEmail_ = accounts[0].email;
    this.primaryAccountIconUrl_ =
        accounts[0].avatarImage || this.primaryAccountIconUrl_;
  }
  // </if>

  /**
   * @return A CSS image-set for multiple scale factors.
   */
  protected getIconImageSet_(iconUrl?: string): string {
    if (!iconUrl) {
      return '';
    }
    return getImage(iconUrl);
  }

  // <if expr="_google_chrome">
  private getScaledSrcSet_(url: string): string {
    return `${url} 1x, ${url}@2x 2x`;
  }

  protected getGooglePayLightModeLogoSrcSet_(): string {
    const logoId = this.autofillEnableGradientGoogleLogosEnabled_ ?
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_WITH_GRADIENT' :
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY';
    return this.getScaledSrcSet_(logoId);
  }

  protected getGooglePayDarkModeLogoSrcSet_(): string {
    const logoId = this.autofillEnableGradientGoogleLogosEnabled_ ?
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_WITH_GRADIENT_DARK' :
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_DARK';
    return this.getScaledSrcSet_(logoId);
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-credit-card-edit-dialog-v2':
        SettingsCreditCardEditDialogV2Element;
  }
}

customElements.define(
    SettingsCreditCardEditDialogV2Element.is,
    SettingsCreditCardEditDialogV2Element);
