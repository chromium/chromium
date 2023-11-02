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
       * An array of all saved UPI Virtual Payment Addresses.
       */
      upiIds: Array,

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
       * True iff both credit cards and UPI IDs will be shown.
       */
      showCreditCardUpiSeparator_: {
        type: Boolean,
        value: false,
        computed: 'computeShowCreditCardUpiSeparator_(' +
            'creditCards, upiIds, enableUpiIds_)',
      },

      /**
       * True iff any payment methods will be shown.
       */
      showAnyPaymentMethods_: {
        type: Boolean,
        value: false,
        computed:
            'computeShowAnyPaymentMethods_(creditCards, upiIds, enableUpiIds_)',
      },
    };
  }

  creditCards: chrome.autofillPrivate.CreditCardEntry[];
  upiIds: string[];
  private enableUpiIds_: boolean;
  private showCreditCardUpiSeparator_: boolean;
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
   * @return true iff both credit cards and UPI IDs will be shown.
   */
  private computeShowCreditCardUpiSeparator_(): boolean {
    return this.showCreditCards_() && this.showUpiIds_();
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
    return this.showCreditCards_() || this.showUpiIds_();
  }
}

customElements.define(
    SettingsPaymentsListElement.is, SettingsPaymentsListElement);
