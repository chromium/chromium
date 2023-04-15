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
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import '../i18n_setup.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './credit_card_edit_dialog.html.js';

/**
 * Regular expression for invalid nickname. Nickname containing any digits will
 * be treated as invalid.
 */
const NICKNAME_INVALID_REGEX: RegExp = new RegExp('.*\\d+.*');

declare global {
  interface HTMLElementEventMap {
    'save-credit-card': CustomEvent<chrome.autofillPrivate.CreditCardEntry>;
  }
}

export interface SettingsCreditCardEditDialogElement {
  $: {
    cancelButton: CrButtonElement,
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
       * The credit card being edited.
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

      name_: String,
      cardNumber_: String,
      nickname_: String,
      expirationYear_: String,
      expirationMonth_: String,

      /** Whether the current nickname input is invalid. */
      nicknameInvalid_: {
        type: Boolean,
        value: false,
      },

      expired_: {
        type: Boolean,
        computed: 'computeExpired_(expirationMonth_, expirationYear_)',
        reflectToAttribute: true,
        observer: 'onExpiredChanged_',
      },
    };
  }

  creditCard: chrome.autofillPrivate.CreditCardEntry;
  private title_: string;
  private monthList_: string[];
  private yearList_: string[];
  private name_?: string;
  private cardNumber_?: string;
  private nickname_?: string;
  private expirationYear_?: string;
  private expirationMonth_?: string;
  private nicknameInvalid_: boolean;
  private expired_: boolean;

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
      this.name_ = this.creditCard.name;
      this.cardNumber_ = this.creditCard.cardNumber;
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
    this.creditCard.cardNumber = this.cardNumber_;
    this.creditCard.nickname = this.nickname_;
    this.trimCreditCard_();
    this.dispatchEvent(new CustomEvent(
        'save-credit-card',
        {bubbles: true, composed: true, detail: this.creditCard}));
    this.close();
  }

  private onMonthChange_() {
    this.expirationMonth_ = this.monthList_[this.$.month.selectedIndex];
  }

  private onYearChange_() {
    this.expirationYear_ = this.yearList_[this.$.year.selectedIndex];
  }

  private saveEnabled_() {
    // The save button is enabled if:
    // There is a name or number for the card
    // and the expiration date is valid
    // and the nickname is valid if present.
    return ((this.name_ && this.name_.trim()) ||
            (this.cardNumber_ && this.cardNumber_.trim())) &&
        !this.expired_ && !this.nicknameInvalid_;
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

  /**
   * @return 'true' or 'false' for the aria-invalid attribute
   *     of expiration selectors.
   */
  private getExpirationAriaInvalid_(): string {
    return this.expired_ ? 'true' : 'false';
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
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-credit-card-edit-dialog': SettingsCreditCardEditDialogElement;
  }
}

customElements.define(
    SettingsCreditCardEditDialogElement.is,
    SettingsCreditCardEditDialogElement);
