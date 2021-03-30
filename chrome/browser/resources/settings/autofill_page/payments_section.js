// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-payments-section' is the section containing saved
 * credit cards for use in autofill and payments APIs.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared_css.js';
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import './credit_card_edit_dialog.js';
import './passwords_shared_css.js';
import './payments_list.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';

import {AutofillManager} from './autofill_section.js';

/** @typedef {chrome.autofillPrivate.CreditCardEntry} */
let CreditCardEntry;

/**
 * Interface for all callbacks to the payments autofill API.
 * @interface
 */
export class PaymentsManager {
  /**
   * Add an observer to the list of personal data.
   * @param {function(!Array<!AutofillManager.AddressEntry>,
   *   !Array<!CreditCardEntry>):void} listener
   */
  setPersonalDataManagerListener(listener) {}

  /**
   * Remove an observer from the list of personal data.
   * @param {function(!Array<!AutofillManager.AddressEntry>,
   *     !Array<!CreditCardEntry>):void} listener
   */
  removePersonalDataManagerListener(listener) {}

  /**
   * Request the list of credit cards.
   * @param {function(!Array<!CreditCardEntry>):void}
   *     callback
   */
  getCreditCardList(callback) {}

  /** @param {string} guid The GUID of the credit card to remove.  */
  removeCreditCard(guid) {}

  /**
   * @param {string} guid The GUID to credit card to remove from the cache.
   */
  clearCachedCreditCard(guid) {}

  /**
   * Saves the given credit card.
   * @param {!CreditCardEntry} creditCard
   */
  saveCreditCard(creditCard) {}

  /**
   * Migrate the local credit cards.
   */
  migrateCreditCards() {}

  /**
   * Logs that the server cards edit link was clicked.
   */
  logServerCardLinkClicked() {}

  /**
   * Enables FIDO authentication for card unmasking.
   */
  setCreditCardFIDOAuthEnabledState(enabled) {}

  /**
   * Requests the list of UPI IDs from personal data.
   * @param {function(!Array<!string>):void} callback
   */
  getUpiIdList(callback) {}
}

/**
 * Implementation that accesses the private API.
 * @implements {PaymentsManager}
 */
export class PaymentsManagerImpl {
  /** @override */
  setPersonalDataManagerListener(listener) {
    chrome.autofillPrivate.onPersonalDataChanged.addListener(listener);
  }

  /** @override */
  removePersonalDataManagerListener(listener) {
    chrome.autofillPrivate.onPersonalDataChanged.removeListener(listener);
  }

  /** @override */
  getCreditCardList(callback) {
    chrome.autofillPrivate.getCreditCardList(callback);
  }

  /** @override */
  removeCreditCard(guid) {
    chrome.autofillPrivate.removeEntry(assert(guid));
  }

  /** @override */
  clearCachedCreditCard(guid) {
    chrome.autofillPrivate.maskCreditCard(assert(guid));
  }

  /** @override */
  saveCreditCard(creditCard) {
    chrome.autofillPrivate.saveCreditCard(creditCard);
  }

  /** @override */
  migrateCreditCards() {
    chrome.autofillPrivate.migrateCreditCards();
  }

  /** @override */
  logServerCardLinkClicked() {
    chrome.autofillPrivate.logServerCardLinkClicked();
  }

  /** @override */
  setCreditCardFIDOAuthEnabledState(enabled) {
    chrome.autofillPrivate.setCreditCardFIDOAuthEnabledState(enabled);
  }

  /** @override */
  getUpiIdList(callback) {
    chrome.autofillPrivate.getUpiIdList(callback);
  }
}

addSingletonGetter(PaymentsManagerImpl);

Polymer({
  is: 'settings-payments-section',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
    I18nBehavior,
  ],

  properties: {
    /**
     * An array of all saved credit cards.
     * @type {!Array<!CreditCardEntry>}
     */
    creditCards: {
      type: Array,
      value: () => [],
    },

    /**
     * An array of all saved UPI IDs.
     * @type {!Array<!string>}
     */
    upiIds: {
      type: Array,
      value: () => [],
    },

    /**
     * Set to true if user can be verified through FIDO authentication.
     * @private
     */
    userIsFidoVerifiable_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean(
            'fidoAuthenticationAvailableForAutofill');
      },
    },

    /**
     * The model for any credit card related action menus or dialogs.
     * @private {?chrome.autofillPrivate.CreditCardEntry}
     */
    activeCreditCard: Object,

    /** @private */
    showCreditCardDialog_: Boolean,

    /** @private */
    migratableCreditCardsInfo_: String,

    /**
     * Whether migration local card on settings page is enabled.
     * @private
     */
    migrationEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('migrationEnabled');
      },
      readOnly: true,
    },
  },

  listeners: {
    'save-credit-card': 'saveCreditCard_',
    'dots-card-menu-click': 'onCreditCardDotsMenuTap_',
    'remote-card-menu-click': 'onRemoteEditCreditCardTap_',
  },

  /**
   * The element to return focus to, when the currently active dialog is
   * closed.
   * @private {?HTMLElement}
   */
  activeDialogAnchor_: null,

  /**
   * @type {PaymentsManager}
   * @private
   */
  PaymentsManager_: null,

  /**
   * @type {?function(!Array<!AutofillManager.AddressEntry>,
   *     !Array<!CreditCardEntry>)}
   * @private
   */
  setPersonalDataListener_: null,

  /** @override */
  attached() {
    // Create listener function.
    /** @type {function(!Array<!CreditCardEntry>)} */
    const setCreditCardsListener = cardList => {
      this.creditCards = cardList;
    };

    // Update |userIsFidoVerifiable_| based on the availability of a platform
    // authenticator.
    if (window.PublicKeyCredential) {
      window.PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable()
          .then(r => {
            this.userIsFidoVerifiable_ = this.userIsFidoVerifiable_ && r;
          });
    }

    /**
     * @type {function(!Array<!AutofillManager.AddressEntry>,
     *     !Array<!CreditCardEntry>)}
     */
    const setPersonalDataListener = (addressList, cardList) => {
      this.creditCards = cardList;
    };

    /** @type {function(!Array<!string>)} */
    const setUpiIdsListener = upiIdList => {
      this.upiIds = upiIdList;
    };

    // Remember the bound reference in order to detach.
    this.setPersonalDataListener_ = setPersonalDataListener;

    // Set the managers. These can be overridden by tests.
    this.paymentsManager_ = PaymentsManagerImpl.getInstance();

    // Request initial data.
    this.paymentsManager_.getCreditCardList(setCreditCardsListener);
    this.paymentsManager_.getUpiIdList(setUpiIdsListener);

    // Listen for changes.
    this.paymentsManager_.setPersonalDataManagerListener(
        setPersonalDataListener);

    // Record that the user opened the payments settings.
    chrome.metricsPrivate.recordUserAction('AutofillCreditCardsViewed');
  },

  /** @override */
  detached() {
    this.paymentsManager_.removePersonalDataManagerListener(
        /**
           @type {function(!Array<!AutofillManager.AddressEntry>,
               !Array<!CreditCardEntry>)}
         */
        (this.setPersonalDataListener_));
  },

  /**
   * Opens the credit card action menu.
   * @param {!CustomEvent<{creditCard:
   *     !chrome.autofillPrivate.CreditCardEntry, anchorElement:
   *     !HTMLElement}>} e
   * @private
   */
  onCreditCardDotsMenuTap_(e) {
    // Copy item so dialog won't update model on cancel.
    this.activeCreditCard = e.detail.creditCard;

    /** @type {!CrActionMenuElement} */ (this.$.creditCardSharedMenu)
        .showAt(e.detail.anchorElement);
    this.activeDialogAnchor_ = e.detail.anchorElement;
  },

  /**
   * Handles tapping on the "Add credit card" button.
   * @param {!Event} e
   * @private
   */
  onAddCreditCardTap_(e) {
    e.preventDefault();
    const date = new Date();  // Default to current month/year.
    const expirationMonth = date.getMonth() + 1;  // Months are 0 based.
    this.activeCreditCard = {
      expirationMonth: expirationMonth.toString(),
      expirationYear: date.getFullYear().toString(),
    };
    this.showCreditCardDialog_ = true;
    this.activeDialogAnchor_ =
        /** @type {HTMLElement} */ (this.$.addCreditCard);
  },

  /** @private */
  onCreditCardDialogClose_() {
    this.showCreditCardDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
    this.activeCreditCard = null;
  },

  /**
   * Handles tapping on the "Edit" credit card button.
   * @param {!Event} e The polymer event.
   * @private
   */
  onMenuEditCreditCardTap_(e) {
    e.preventDefault();

    if (this.activeCreditCard.metadata.isLocal) {
      this.showCreditCardDialog_ = true;
    } else {
      this.onRemoteEditCreditCardTap_();
    }

    this.$.creditCardSharedMenu.close();
  },

  /** @private */
  onRemoteEditCreditCardTap_() {
    this.paymentsManager_.logServerCardLinkClicked();
    window.open(loadTimeData.getString('manageCreditCardsUrl'));
  },

  /**
   * Handles tapping on the "Remove" credit card button.
   * @private
   */
  onMenuRemoveCreditCardTap_() {
    this.paymentsManager_.removeCreditCard(
        /** @type {string} */ (this.activeCreditCard.guid));
    this.$.creditCardSharedMenu.close();
    this.activeCreditCard = null;
  },

  /**
   * Handles tapping on the "Clear copy" button for cached credit cards.
   * @private
   */
  onMenuClearCreditCardTap_() {
    this.paymentsManager_.clearCachedCreditCard(
        /** @type {string} */ (this.activeCreditCard.guid));
    this.$.creditCardSharedMenu.close();
    this.activeCreditCard = null;
  },

  /**
   * Handles clicking on the "Migrate" button for migrate local credit
   * cards.
   * @private
   */
  onMigrateCreditCardsClick_() {
    this.paymentsManager_.migrateCreditCards();
  },

  /**
   * Records changes made to the "Allow sites to check if you have payment
   * methods saved" setting to a histogram.
   * @private
   */
  onCanMakePaymentChange_() {
    MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        PrivacyElementInteractions.PAYMENT_METHOD);
  },

  /**
   * Listens for the save-credit-card event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  saveCreditCard_(event) {
    this.paymentsManager_.saveCreditCard(event.detail);
  },

  /**
   * @param {boolean} creditCardEnabled
   * @return {boolean} Whether or not the user is verifiable through FIDO
   *     authentication.
   * @private
   */
  shouldShowFidoToggle_(creditCardEnabled, userIsFidoVerifiable) {
    return creditCardEnabled && userIsFidoVerifiable;
  },

  /**
   * Listens for the enable-authentication event, and calls the private API.
   * @private
   */
  setFIDOAuthenticationEnabledState_() {
    this.paymentsManager_.setCreditCardFIDOAuthEnabledState(
        this.$$('#autofillCreditCardFIDOAuthToggle').checked);
  },

  /**
   * @param {!Array<!CreditCardEntry>} creditCards
   * @param {boolean} creditCardEnabled
   * @return {boolean} Whether to show the migration button.
   * @private
   */
  checkIfMigratable_(creditCards, creditCardEnabled) {
    // If migration prerequisites are not met, return false.
    if (!this.migrationEnabled_) {
      return false;
    }

    // If credit card enabled pref is false, return false.
    if (!creditCardEnabled) {
      return false;
    }

    const numberOfMigratableCreditCard =
        creditCards.filter(card => card.metadata.isMigratable).length;
    // Check whether exist at least one local valid card for migration.
    if (numberOfMigratableCreditCard === 0) {
      return false;
    }

    // Update the display text depends on the number of migratable credit
    // cards.
    this.migratableCreditCardsInfo_ = numberOfMigratableCreditCard === 1 ?
        this.i18n('migratableCardsInfoSingle') :
        this.i18n('migratableCardsInfoMultiple');

    return true;
  },

});
