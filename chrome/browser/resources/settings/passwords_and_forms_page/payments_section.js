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
   * Add an observer to the list of credit cards.
   * @param {function(!Array<!PaymentsManager.CreditCardEntry>):void} listener
   */
  addCreditCardListChangedListener(listener) {}

  /**
   * Remove an observer from the list of credit cards.
   * @param {function(!Array<!PaymentsManager.CreditCardEntry>):void} listener
   */
  removeCreditCardListChangedListener(listener) {}

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
}

/** @typedef {chrome.autofillPrivate.CreditCardEntry} */
PaymentsManager.CreditCardEntry;

/**
 * Implementation that accesses the private API.
 * @implements {PaymentsManager}
 */
class PaymentsManagerImpl {
  /** @override */
  addCreditCardListChangedListener(listener) {
    chrome.autofillPrivate.onCreditCardListChanged.addListener(listener);
  }

  /** @override */
  removeCreditCardListChangedListener(listener) {
    chrome.autofillPrivate.onCreditCardListChanged.removeListener(listener);
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
     * An array of saved credit cards.
     * @type {!Array<!PaymentsManager.CreditCardEntry>}
     */
    creditCards: Array,

    /**
     * The model for any credit card related action menus or dialogs.
     * @private {?chrome.autofillPrivate.CreditCardEntry}
     */
    activeCreditCard: Object,

    /** @private */
    showCreditCardDialog_: Boolean,

    /** @private */
    migrateCreditCardsLabel_: String,

    /** @private */
    migratableCreditCardsInfo_: String,

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

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

    /**
     * Whether user has a Google Payments account.
     * @private
     */
    hasGooglePaymentsAccount_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('hasGooglePaymentsAccount');
      },
      readOnly: true,
    },

    /**
     * Whether Autofill Upstream is enabled.
     * @private
     */
    upstreamEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('upstreamEnabled');
      },
      readOnly: true,
    },

    /**
     * Whether the user has a secondary sync passphrase.
     * @private
     */
    isUsingSecondaryPassphrase_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isUsingSecondaryPassphrase');
      },
      readOnly: true,
    },

    /**
     * Whether the upload-to-google state is active.
     * @private
     */
    uploadToGoogleActive_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('uploadToGoogleActive');
      },
      readOnly: true,
    },

    /**
     * Whether the domain of the user's email is allowed.
     * @private
     */
    userEmailDomainAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('userEmailDomainAllowed');
      },
      readOnly: true,
    },
  },

  listeners: {
    'save-credit-card': 'saveCreditCard_',
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
   * @type {?function(!Array<!PaymentsManager.CreditCardEntry>)}
   * @private
   */
  setCreditCardsListener_: null,

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  attached: function() {
    // Create listener function.
    /** @type {function(!Array<!PaymentsManager.CreditCardEntry>)} */
    const setCreditCardsListener = list => {
      this.creditCards = list;
    };

    // Remember the bound reference in order to detach.
    this.setCreditCardsListener_ = setCreditCardsListener;

    // Set the managers. These can be overridden by tests.
    this.paymentsManager_ = PaymentsManagerImpl.getInstance();

    // Request initial data.
    this.paymentsManager_.getCreditCardList(setCreditCardsListener);

    // Listen for changes.
    this.paymentsManager_.addCreditCardListChangedListener(
        setCreditCardsListener);

    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  },

  /** @override */
  detached: function() {
    this.paymentsManager_.removeCreditCardListChangedListener(
        /** @type {function(!Array<!PaymentsManager.CreditCardEntry>)} */ (
            this.setCreditCardsListener_));
  },

  /**
   * Formats the expiration date so it's displayed as MM/YYYY.
   * @param {!chrome.autofillPrivate.CreditCardEntry} item
   * @return {string}
   * @private
   */
  expiration_: function(item) {
    return item.expirationMonth + '/' + item.expirationYear;
  },

  /**
   * Opens the credit card action menu.
   * @param {!Event} e The polymer event.
   * @private
   */
  onCreditCardMenuTap_: function(e) {
    const menuEvent = /** @type {!{model: !{item: !Object}}} */ (e);

    /* TODO(scottchen): drop the [dataHost][dataHost] once this bug is fixed:
     https://github.com/Polymer/polymer/issues/2574 */
    // TODO(dpapad): The [dataHost][dataHost] workaround is only necessary for
    // Polymer 1. Remove once migration to Polymer 2 has completed.
    const item = Polymer.DomIf ? menuEvent.model.item :
                                 menuEvent.model['dataHost']['dataHost'].item;

    // Copy item so dialog won't update model on cancel.
    this.activeCreditCard =
        /** @type {!chrome.autofillPrivate.CreditCardEntry} */ (
            Object.assign({}, item));

    const dotsButton = /** @type {!HTMLElement} */ (Polymer.dom(e).localTarget);
    /** @type {!CrActionMenuElement} */ (this.$.creditCardSharedMenu)
        .showAt(dotsButton);
    this.activeDialogAnchor_ = dotsButton;
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

    if (this.activeCreditCard.metadata.isLocal)
      this.showCreditCardDialog_ = true;
    else
      this.onRemoteEditCreditCardTap_();

    this.$.creditCardSharedMenu.close();
  },

  /** @private */
  onRemoteEditCreditCardTap_: function() {
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
   * Handles clicking on the "Migrate" button for migrate local credit cards.
   * @private
   */
  onMigrateCreditCardsClick_: function() {
    this.paymentsManager_.migrateCreditCards();
  },

  /**
   * The 3-dot menu should not be shown if the card is entirely remote.
   * @param {!chrome.autofillPrivate.AutofillMetadata} metadata
   * @return {boolean}
   * @private
   */
  showDots_: function(metadata) {
    return !!(metadata.isLocal || metadata.isCached);
  },

  /**
   * Returns true if the list exists and has items.
   * @param {Array<Object>} list
   * @return {boolean}
   * @private
   */
  hasSome_: function(list) {
    return !!(list && list.length);
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
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_: function(syncStatus) {
    this.syncStatus = syncStatus;
  },

  /**
   * @param {!settings.SyncStatus} syncStatus
   * @param {!Array<!PaymentsManager.CreditCardEntry>} creditCards
   * @param {boolean} creditCardEnabled
   * @return {boolean} Whether to show the migration button. True iff at least
   * one valid local card, enable migration, signed-in & synced and credit card
   * pref enabled.
   * @private
   */
  checkIfMigratable_: function(syncStatus, creditCards, creditCardEnabled) {
    if (syncStatus == undefined)
      return false;

    // If user not enable migration experimental flag, return false.
    if (!this.migrationEnabled_)
      return false;

    // If user does not have Google Payments Account, return false.
    if (!this.hasGooglePaymentsAccount_)
      return false;

    // If the Autofill Upstream feature is not enabled, return false.
    if (!this.upstreamEnabled_)
      return false;

    // Don't offer upload if user has a secondary passphrase. Users who have
    // enabled a passphrase have chosen to not make their sync information
    // accessible to Google. Since upload makes credit card data available
    // to other Google systems, disable it for passphrase users.
    if (this.isUsingSecondaryPassphrase_)
      return false;

    // If upload-to-Google state is not active, card cannot be saved to Google
    // Payments. Return false.
    if (!this.uploadToGoogleActive_)
      return false;

    // The domain of the user's email address is not allowed, return false.
    if (!this.userEmailDomainAllowed_)
      return false;

    // If credit card enabled pref is false, return false.
    if (!creditCardEnabled)
      return false;

    // If user not signed-in and synced, return false.
    if (!syncStatus.signedIn || !syncStatus.syncSystemEnabled)
      return false;

    let numberOfMigratableCreditCard =
        creditCards.filter(card => card.metadata.isMigratable).length;
    // Check whether exist at least one local valid card for migration.
    if (numberOfMigratableCreditCard == 0)
      return false;

    // Update the display label depends on the number of migratable credit
    // cards.
    this.migrateCreditCardsLabel_ = numberOfMigratableCreditCard == 1 ?
        this.i18n('migrateCreditCardsLabelSingle') :
        this.i18n('migrateCreditCardsLabelMultiple');
    // Update the display text depends on the number of migratable credit cards.
    this.migratableCreditCardsInfo_ = numberOfMigratableCreditCard == 1 ?
        this.i18n('migratableCardsInfoSingle') :
        this.i18n('migratableCardsInfoMultiple');

    return true;
  },

});
})();
