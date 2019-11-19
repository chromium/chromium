// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-payments-section' is the section containing saved
 * credit cards for use in autofill and payments APIs.
 */

/**
 * Interface for all callbacks to the payments autofill API.
 * @interface
 */
class PaymentsManager {
  /**
   * Add an observer to the list of personal data.
   * @param {function(!Array<!AutofillManager.AddressEntry>,
   *   !Array<!PaymentsManager.CreditCardEntry>):void} listener
   */
  setPersonalDataManagerListener(listener) {}

  /**
   * Remove an observer from the list of personal data.
   * @param {function(!Array<!AutofillManager.AddressEntry>,
   *     !Array<!PaymentsManager.CreditCardEntry>):void} listener
   */
  removePersonalDataManagerListener(listener) {}

  /**
   * Request the list of credit cards.
   * @param {function(!Array<!PaymentsManager.CreditCardEntry>):void} callback
   */
  getCreditCardList(callback) {}

  /** @param {string} guid The GUID of the credit card to remove.  */
  removeCreditCard(guid) {}

  /** @param {string} guid The GUID to credit card to remove from the cache. */
  clearCachedCreditCard(guid) {}

  /**
   * Saves the given credit card.
   * @param {!PaymentsManager.CreditCardEntry} creditCard
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
}

/** @typedef {chrome.autofillPrivate.CreditCardEntry} */
PaymentsManager.CreditCardEntry;

/**
 * Implementation that accesses the private API.
 * @implements {PaymentsManager}
 */
class PaymentsManagerImpl {
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
}

cr.addSingletonGetter(PaymentsManagerImpl);

(function() {
'use strict';

Polymer({
  is: 'settings-payments-section',

  behaviors: [
    WebUIListenerBehavior,
    I18nBehavior,
  ],

  properties: {
    /**
     * An array of all saved credit cards.
     * @type {!Array<!PaymentsManager.CreditCardEntry>}
     */
    creditCards: {
      type: Array,
      value: () => [],
    },

    /**
     * Set to true if user can be verified through FIDO authentication.
     * @private
     */
    userIsFidoVerifiable_: {
      type: Boolean,
      value: function() {
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
      value: function() {
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
   *     !Array<!PaymentsManager.CreditCardEntry>)}
   * @private
   */
  setPersonalDataListener_: null,

  /** @override */
  attached: function() {
    // Create listener function.
    /** @type {function(!Array<!PaymentsManager.CreditCardEntry>)} */
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
     *     !Array<!PaymentsManager.CreditCardEntry>)}
     */
    const setPersonalDataListener = (addressList, cardList) => {
      this.creditCards = cardList;
    };

    // Remember the bound reference in order to detach.
    this.setPersonalDataListener_ = setPersonalDataListener;

    // Set the managers. These can be overridden by tests.
    this.paymentsManager_ = PaymentsManagerImpl.getInstance();

    // Request initial data.
    this.paymentsManager_.getCreditCardList(setCreditCardsListener);

    // Listen for changes.
    this.paymentsManager_.setPersonalDataManagerListener(
        setPersonalDataListener);

    // Record that the user opened the payments settings.
    chrome.metricsPrivate.recordUserAction('AutofillCreditCardsViewed');
  },

  /** @override */
  detached: function() {
    this.paymentsManager_.removePersonalDataManagerListener(
        /**
           @type {function(!Array<!AutofillManager.AddressEntry>,
               !Array<!PaymentsManager.CreditCardEntry>)}
         */
        (this.setPersonalDataListener_));
  },

  /**
   * Opens the credit card action menu.
   * @param {!CustomEvent<{creditCard: !chrome.autofillPrivate.CreditCardEntry,
   *     anchorElement: !HTMLElement}>} e
   * @private
   */
  onCreditCardDotsMenuTap_: function(e) {
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
  onAddCreditCardTap_: function(e) {
    e.preventDefault();
    const date = new Date();  // Default to current month/year.
    const expirationMonth = date.getMonth() + 1;  // Months are 0 based.
    this.activeCreditCard = {
      expirationMonth: expirationMonth.toString(),
      expirationYear: date.getFullYear().toString(),
    };
    this.showCreditCardDialog_ = true;
    this.activeDialogAnchor_ = this.$.addCreditCard;
  },

  /** @private */
  onCreditCardDialogClose_: function() {
    this.showCreditCardDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
    this.activeCreditCard = null;
  },

  /**
   * Handles tapping on the "Edit" credit card button.
   * @param {!Event} e The polymer event.
   * @private
   */
  onMenuEditCreditCardTap_: function(e) {
    e.preventDefault();

    if (this.activeCreditCard.metadata.isLocal) {
      this.showCreditCardDialog_ = true;
    } else {
      this.onRemoteEditCreditCardTap_();
    }

    this.$.creditCardSharedMenu.close();
  },

  /** @private */
  onRemoteEditCreditCardTap_: function() {
    this.paymentsManager_.logServerCardLinkClicked();
    window.open(loadTimeData.getString('manageCreditCardsUrl'));
  },

  /**
   * Handles tapping on the "Remove" credit card button.
   * @private
   */
  onMenuRemoveCreditCardTap_: function() {
    this.paymentsManager_.removeCreditCard(
        /** @type {string} */ (this.activeCreditCard.guid));
    this.$.creditCardSharedMenu.close();
    this.activeCreditCard = null;
  },

  /**
   * Handles tapping on the "Clear copy" button for cached credit cards.
   * @private
   */
  onMenuClearCreditCardTap_: function() {
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
  onMigrateCreditCardsClick_: function() {
    this.paymentsManager_.migrateCreditCards();
  },

  /**
   * Listens for the save-credit-card event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  saveCreditCard_: function(event) {
    this.paymentsManager_.saveCreditCard(event.detail);
  },

  /**
   * @param {boolean} creditCardEnabled
   * @return {boolean} Whether or not the user is verifiable through FIDO
   *     authentication.
   * @private
   */
  shouldShowFidoToggle_: function(creditCardEnabled, userIsFidoVerifiable) {
    return creditCardEnabled && userIsFidoVerifiable;
  },

  /**
   * Listens for the enable-authentication event, and calls the private API.
   * @private
   */
  setFIDOAuthenticationEnabledState_: function() {
    this.paymentsManager_.setCreditCardFIDOAuthEnabledState(
        this.$$('#autofillCreditCardFIDOAuthToggle').checked);
  },

  /**
   * @param {!Array<!PaymentsManager.CreditCardEntry>} creditCards
   * @param {boolean} creditCardEnabled
   * @return {boolean} Whether to show the migration button.
   * @private
   */
  checkIfMigratable_: function(creditCards, creditCardEnabled) {
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
    if (numberOfMigratableCreditCard == 0) {
      return false;
    }

    // Update the display text depends on the number of migratable credit
    // cards.
    this.migratableCreditCardsInfo_ = numberOfMigratableCreditCard == 1 ?
        this.i18n('migratableCardsInfoSingle') :
        this.i18n('migratableCardsInfoMultiple');

    return true;
  },

});
})();
