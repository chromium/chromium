// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'payments-list' is a list of saved payment methods (credit
 * cards etc.) to be shown in the settings page.
 */

import './credit_card_list_entry.js';
import './iban_list_entry.js';
import './pay_over_time_issuer_list_entry.js';

import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';

import type {SettingsCreditCardListEntryElement} from './credit_card_list_entry.js';
import type {SettingsIbanListEntryElement} from './iban_list_entry.js';
import {getCss} from './payments_list.css.js';
import {getHtml} from './payments_list.html.js';

export class SettingsPaymentsListElement extends CrLitElement {
  static get is() {
    return 'settings-payments-list';
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
       * An array of all saved credit cards.
       */
      creditCards: {type: Array},

      /**
       * An array of all saved IBANs.
       */
      ibans: {type: Array},

      /**
       * An array of all saved Pay Over Time issuers.
       */
      payOverTimeIssuers: {type: Array},

      /**
       * True if displaying IBANs in settings is enabled.
       */
      enableIbans_: {type: Boolean},

      /**
       * True if displaying Pay Over Time in settings is enabled.
       */
      enablePayOverTime_: {type: Boolean},
    };
  }

  accessor creditCards: chrome.autofillPrivate.CreditCardEntry[] = [];
  accessor ibans: chrome.autofillPrivate.IbanEntry[] = [];
  accessor payOverTimeIssuers: chrome.autofillPrivate.PayOverTimeIssuerEntry[] =
      [];
  private accessor enableIbans_: boolean =
      loadTimeData.getBoolean('showIbansSettings');
  private accessor enablePayOverTime_: boolean =
      loadTimeData.getBoolean('shouldShowPayOverTimeSettings');

  /**
   * Focuses the next most appropriate element after removing a specific
   * credit card. Returns `false` if it could not find such an element,
   * in this case the focus is supposed to be handled by someone else.
   */
  updateFocusBeforeCreditCardRemoval(cardIndex: number): boolean {
    // The focused element is to be reset only if the last element is deleted,
    // when the number of "dom-repeat" nodes changes and the focus get lost.
    if (cardIndex === this.creditCards.length - 1) {
      return this.updateFocusBeforeRemoval_(this.getCreditCardId_(cardIndex));
    } else {
      return true;
    }
  }

  /**
   * Focuses the next most appropriate element after removing a specific
   * iban. Returns `false` if it could not find such an element,
   * in this case the focus is supposed to be handled by someone else.
   */
  updateFocusBeforeIbanRemoval(ibanIndex: number): boolean {
    // The focused element is to be reset only if the last element is deleted,
    // when the number of "dom-repeat" nodes changes and the focus get lost.
    if (ibanIndex === this.ibans.length - 1) {
      return this.updateFocusBeforeRemoval_(this.getIbanId_(ibanIndex));
    } else {
      return true;
    }
  }

  /**
   * Handles focus resetting across all payment method lists. Returns `false`
   * only when the last payment method is removed, in other cases sets the focus
   * to either the next or previous payment method.
   */
  private updateFocusBeforeRemoval_(id: string): boolean {
    const paymentMethods =
        this.shadowRoot.querySelectorAll<SettingsCreditCardListEntryElement|
                                         SettingsIbanListEntryElement>(
            '.payment-method');

    if (paymentMethods.length <= 1) {
      return false;
    }

    const index = [...paymentMethods].findIndex((element) => element.id === id);
    const isLastItem = index === paymentMethods.length - 1;
    const indexToFocus = index + (isLastItem ? -1 : 1);
    const menu = paymentMethods[indexToFocus].dotsMenu;
    if (menu) {
      focusWithoutInk(menu);
      return true;
    }

    return false;
  }

  protected getCreditCardId_(index: number): string {
    return `card-${index}`;
  }

  protected getIbanId_(index: number): string {
    return `iban-${index}`;
  }

  /**
   * @return true iff any payment methods will be shown.
   */
  protected showAnyPaymentMethods_(): boolean {
    return this.creditCards.length > 0 ||
        (this.enableIbans_ && this.ibans.length > 0) ||
        (this.enablePayOverTime_ && this.payOverTimeIssuers.length > 0);
  }
}

export type PaymentsListElement = SettingsPaymentsListElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-payments-list': SettingsPaymentsListElement;
  }
}

customElements.define(
    SettingsPaymentsListElement.is, SettingsPaymentsListElement);
