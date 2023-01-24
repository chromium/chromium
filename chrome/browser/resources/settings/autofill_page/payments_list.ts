// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'payments-list' is a list of saved payment methods (credit
 * cards etc.) to be shown in the settings page.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import './credit_card_list_entry.js';
import './iban_list_entry.js';
import './passwords_shared.css.js';
import './upi_id_list_entry.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './payments_list.html.js';

class SettingsPaymentsListElement extends PolymerElement {
  static get is() {
    return 'settings-payments-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * An array of all saved credit cards.
       */
      creditCards: Array,

      /**
       * An array of all saved IBANs.
       */
      ibans: Array,

      /**
       * An array of all saved UPI Virtual Payment Addresses.
       */
      upiIds: Array,

      /**
       * True if displaying IBANs in settings is enabled.
       */
      enableIbans_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showIbansSettings');
        },
      },

      /**
       * True if displaying UPI IDs in settings is enabled.
       */
      enableUpiIds_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showUpiIdSettings');
        },
      },

      /**
       * Whether the removal of Expiration and Type titles on settings page is
       * enabled.
       */
      removeCardExpirationAndTypeTitlesEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('removeCardExpirationAndTypeTitles');
        },
        readOnly: true,
      },

      /**
       * True iff both credit cards and IBANs will be shown.
       */
      showCreditCardIbanSeparator_: {
        type: Boolean,
        value: false,
        computed: 'computeShowCreditCardIbanSeparator_(' +
            'creditCards, ibans, enableIbans_)',
      },

      /**
       * True if at least credit cards or IBANs will be shown before UPI IDs
       * section.
       */
      showSeparatorBeforeUpiSection_: {
        type: Boolean,
        value: false,
        computed: 'computeShowSeparatorBeforeUpiSection_(' +
            'creditCards, ibans, enableIbans_, upiIds, enableUpiIds_)',
      },

      /**
       * True iff any payment methods will be shown.
       */
      showAnyPaymentMethods_: {
        type: Boolean,
        value: false,
        computed: 'computeShowAnyPaymentMethods_(' +
            'creditCards, ibans, upiIds, enableIbans_, enableUpiIds_)',
      },
    };
  }

  creditCards: chrome.autofillPrivate.CreditCardEntry[];
  ibans: chrome.autofillPrivate.IbanEntry[];
  upiIds: string[];
  private enableIbans_: boolean;
  private enableUpiIds_: boolean;
  private removeCardExpirationAndTypeTitlesEnabled_: boolean;
  private showCreditCardIbanSeparator_: boolean;
  private showSeparatorBeforeUpiSection_: boolean;
  private showAnyPaymentMethods_: boolean;

  /**
   * @return Whether the list exists and has items.
   */
  private hasSome_(list: any[]): boolean {
    return !!(list && list.length);
  }

  /**
   * @return true iff there are credit cards to be shown.
   */
  private showCreditCards_(): boolean {
    return this.hasSome_(this.creditCards);
  }

  /**
   * @return true if expiration and type titles should be removed.
   */
  private shouldHideExpirationAndTypeTitles_(): boolean {
    return this.removeCardExpirationAndTypeTitlesEnabled_ ||
        !(this.showCreditCards_() || this.showUpiIds_());
  }

  /**
   * @return true iff there are IBANs to be shown.
   */
  private showIbans_(): boolean {
    return this.enableIbans_ && this.hasSome_(this.ibans);
  }

  /**
   * @return true iff both credit cards and IBANs will be shown.
   */
  private computeShowCreditCardIbanSeparator_(): boolean {
    return this.showCreditCards_() && this.showIbans_();
  }

  /**
   * @return true iff both credit cards and UPI IDs will be shown, or both IBANs
   *     and UPI IDs will be shown.
   */
  private computeShowSeparatorBeforeUpiSection_(): boolean {
    return (this.showCreditCards_() || this.showIbans_()) && this.showUpiIds_();
  }

  /**
   * @return true iff there UPI IDs and they should be shown.
   */
  private showUpiIds_(): boolean {
    return this.enableUpiIds_ && this.hasSome_(this.upiIds);
  }

  /**
   * @return true iff any payment methods will be shown.
   */
  private computeShowAnyPaymentMethods_(): boolean {
    return this.showCreditCards_() || this.showIbans_() || this.showUpiIds_();
  }
}

customElements.define(
    SettingsPaymentsListElement.is, SettingsPaymentsListElement);
