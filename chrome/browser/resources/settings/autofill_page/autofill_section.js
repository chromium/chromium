// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-section' is the section containing saved
 * addresses for use in autofill and payments APIs.
 */

/**
 * Interface for all callbacks to the autofill API.
 * @interface
 */
class AutofillManager {
  /**
   * Add an observer to the list of personal data.
   * @param {function(!Array<!AutofillManager.AddressEntry>,
   *     !Array<!PaymentsManager.CreditCardEntry>):void} listener
   */
  setPersonalDataManagerListener(listener) {}

  /**
   * Remove an observer from the list of personal data.
   * @param {function(!Array<!AutofillManager.AddressEntry>,
   *     !Array<!PaymentsManager.CreditCardEntry>):void} listener
   */
  removePersonalDataManagerListener(listener) {}

  /**
   * Request the list of addresses.
   * @param {function(!Array<!AutofillManager.AddressEntry>):void} callback
   */
  getAddressList(callback) {}

  /**
   * Saves the given address.
   * @param {!AutofillManager.AddressEntry} address
   */
  saveAddress(address) {}

  /** @param {string} guid The guid of the address to remove.  */
  removeAddress(guid) {}
}

/** @typedef {chrome.autofillPrivate.AddressEntry} */
AutofillManager.AddressEntry;

/**
 * Implementation that accesses the private API.
 * @implements {AutofillManager}
 */
class AutofillManagerImpl {
  /** @override */
  setPersonalDataManagerListener(listener) {
    chrome.autofillPrivate.onPersonalDataChanged.addListener(listener);
  }

  /** @override */
  removePersonalDataManagerListener(listener) {
    chrome.autofillPrivate.onPersonalDataChanged.removeListener(listener);
  }

  /** @override */
  getAddressList(callback) {
    chrome.autofillPrivate.getAddressList(callback);
  }

  /** @override */
  saveAddress(address) {
    chrome.autofillPrivate.saveAddress(address);
  }

  /** @override */
  removeAddress(guid) {
    chrome.autofillPrivate.removeEntry(assert(guid));
  }
}

cr.addSingletonGetter(AutofillManagerImpl);

(function() {
'use strict';

Polymer({
  is: 'settings-autofill-section',

  properties: {
    /**
     * An array of saved addresses.
     * @type {!Array<!AutofillManager.AddressEntry>}
     */
    addresses: Array,

    /**
     * The model for any address related action menus or dialogs.
     * @private {?chrome.autofillPrivate.AddressEntry}
     */
    activeAddress: Object,

    /** @private */
    showAddressDialog_: Boolean,
  },

  listeners: {
    'save-address': 'saveAddress_',
  },

  /**
   * The element to return focus to, when the currently active dialog is
   * closed.
   * @private {?HTMLElement}
   */
  activeDialogAnchor_: null,

  /**
   * @type {AutofillManager}
   * @private
   */
  autofillManager_: null,

  /**
   * @type {?function(!Array<!AutofillManager.AddressEntry>,
   *     !Array<!PaymentsManager.CreditCardEntry>)}
   * @private
   */
  setPersonalDataListener_: null,

  /** @override */
  attached: function() {
    // Create listener functions.
    /** @type {function(!Array<!AutofillManager.AddressEntry>)} */
    const setAddressesListener = addressList => {
      this.addresses = addressList;
    };

    /**
     * @type {function(!Array<!AutofillManager.AddressEntry>,
     *     !Array<!PaymentsManager.CreditCardEntry>)}
     */
    const setPersonalDataListener = (addressList, cardList) => {
      this.addresses = addressList;
    };

    // Remember the bound reference in order to detach.
    this.setPersonalDataListener_ = setPersonalDataListener;

    // Set the managers. These can be overridden by tests.
    this.autofillManager_ = AutofillManagerImpl.getInstance();

    // Request initial data.
    this.autofillManager_.getAddressList(setAddressesListener);

    // Listen for changes.
    this.autofillManager_.setPersonalDataManagerListener(
        setPersonalDataListener);

    // Record that the user opened the address settings.
    chrome.metricsPrivate.recordUserAction('AutofillAddressesViewed');
  },

  /** @override */
  detached: function() {
    this.autofillManager_.removePersonalDataManagerListener(
        /**
           @type {function(!Array<!AutofillManager.AddressEntry>,
               !Array<!PaymentsManager.CreditCardEntry>)}
         */
        (this.setPersonalDataListener_));
  },

  /**
   * Open the address action menu.
   * @param {!Event} e The polymer event.
   * @private
   */
  onAddressMenuTap_: function(e) {
    const menuEvent = /** @type {!{model: !{item: !Object}}} */ (e);
    const item = menuEvent.model.item;

    // Copy item so dialog won't update model on cancel.
    this.activeAddress = /** @type {!chrome.autofillPrivate.AddressEntry} */ (
        Object.assign({}, item));

    const dotsButton = /** @type {!HTMLElement} */ (e.target);
    /** @type {!CrActionMenuElement} */ (this.$.addressSharedMenu)
        .showAt(dotsButton);
    this.activeDialogAnchor_ = dotsButton;
  },

  /**
   * Handles tapping on the "Add address" button.
   * @param {!Event} e The polymer event.
   * @private
   */
  onAddAddressTap_: function(e) {
    e.preventDefault();
    this.activeAddress = {};
    this.showAddressDialog_ = true;
    this.activeDialogAnchor_ = this.$.addAddress;
  },

  /** @private */
  onAddressDialogClose_: function() {
    this.showAddressDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
  },

  /**
   * Handles tapping on the "Edit" address button.
   * @param {!Event} e The polymer event.
   * @private
   */
  onMenuEditAddressTap_: function(e) {
    e.preventDefault();
    this.showAddressDialog_ = true;
    this.$.addressSharedMenu.close();
  },

  /** @private */
  onRemoteEditAddressTap_: function() {
    window.open(loadTimeData.getString('manageAddressesUrl'));
  },

  /**
   * Handles tapping on the "Remove" address button.
   * @private
   */
  onMenuRemoveAddressTap_: function() {
    this.autofillManager_.removeAddress(
        /** @type {string} */ (this.activeAddress.guid));
    this.$.addressSharedMenu.close();
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
   * Listens for the save-address event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  saveAddress_: function(event) {
    this.autofillManager_.saveAddress(event.detail);
  },
});
})();
