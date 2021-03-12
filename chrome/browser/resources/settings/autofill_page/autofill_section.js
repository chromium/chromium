// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-section' is the section containing saved
 * addresses for use in autofill and payments APIs.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared_css.js';
import '../controls/extension_controlled_indicator.js';
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import './address_edit_dialog.js';
import './address_remove_confirmation_dialog.js';
import './passwords_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

/** @typedef {chrome.autofillPrivate.CreditCardEntry} */
let CreditCardEntry;

/**
 * Interface for all callbacks to the autofill API.
 * @interface
 */
export class AutofillManager {
  /**
   * Add an observer to the list of personal data.
   * @param {function(!Array<!AutofillManager.AddressEntry>,
   *     !Array<!CreditCardEntry>):void} listener
   */
  setPersonalDataManagerListener(listener) {}

  /**
   * Remove an observer from the list of personal data.
   * @param {function(!Array<!AutofillManager.AddressEntry>,
   *     !Array<!CreditCardEntry>):void} listener
   */
  removePersonalDataManagerListener(listener) {}

  /**
   * Request the list of addresses.
   * @param {function(!Array<!AutofillManager.AddressEntry>):void}
   *     callback
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
export class AutofillManagerImpl {
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

addSingletonGetter(AutofillManagerImpl);

Polymer({
  is: 'settings-autofill-section',

  _template: html`{__html_template__}`,

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

    /** @private */
    showAddressRemoveConfirmationDialog_: Boolean,
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
   *     !Array<!CreditCardEntry>)}
   * @private
   */
  setPersonalDataListener_: null,

  /** @override */
  attached() {
    // Create listener functions.
    /** @type {function(!Array<!AutofillManager.AddressEntry>)} */
    const setAddressesListener = addressList => {
      this.addresses = addressList;
    };

    /**
     * @type {function(!Array<!AutofillManager.AddressEntry>,
     *     !Array<!CreditCardEntry>)}
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
  detached() {
    this.autofillManager_.removePersonalDataManagerListener(
        /**
           @type {function(!Array<!AutofillManager.AddressEntry>,
               !Array<!CreditCardEntry>)}
         */
        (this.setPersonalDataListener_));
  },

  /**
   * Open the address action menu.
   * @param {!Event} e The polymer event.
   * @private
   */
  onAddressMenuTap_(e) {
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
  onAddAddressTap_(e) {
    e.preventDefault();
    this.activeAddress = {};
    this.showAddressDialog_ = true;
    this.activeDialogAnchor_ = /** @type {HTMLElement} */ (this.$.addAddress);
  },

  /** @private */
  onAddressDialogClose_() {
    this.showAddressDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
  },

  /**
   * Handles tapping on the "Edit" address button.
   * @param {!Event} e The polymer event.
   * @private
   */
  onMenuEditAddressTap_(e) {
    e.preventDefault();
    this.showAddressDialog_ = true;
    this.$.addressSharedMenu.close();
  },

  /** @private */
  onAddressRemoveConfirmationDialogClose_: function() {
    // Check if the dialog was confirmed before closing it.
    if (/** @type {!SettingsAddressRemoveConfirmationDialogElement} */
        (this.$$('settings-address-remove-confirmation-dialog'))
            .wasConfirmed()) {
      this.autofillManager_.removeAddress(
          /** @type {string} */ (this.activeAddress.guid));
    }
    this.showAddressRemoveConfirmationDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
  },

  /**
   * Handles tapping on the "Remove" address button.
   * @private
   */
  onMenuRemoveAddressTap_() {
    this.showAddressRemoveConfirmationDialog_ = true;
    this.$.addressSharedMenu.close();
  },

  /**
   * Returns true if the list exists and has items.
   * @param {Array<Object>} list
   * @return {boolean}
   * @private
   */
  hasSome_(list) {
    return !!(list && list.length);
  },

  /**
   * Listens for the save-address event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  saveAddress_(event) {
    this.autofillManager_.saveAddress(event.detail);
  },
});
