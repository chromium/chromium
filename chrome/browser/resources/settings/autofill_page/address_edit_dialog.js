// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'address-edit-dialog' is the dialog that allows editing a saved
 * address.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';
import '../controls/settings_textarea.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {flush, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

Polymer({
  is: 'settings-address-edit-dialog',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @type {chrome.autofillPrivate.AddressEntry} */
    address: Object,

    /** @private */
    title_: String,

    /** @private {!Array<!chrome.autofillPrivate.CountryEntry>} */
    countries_: Array,

    /**
     * Updates the address wrapper.
     * @private {string|undefined}
     */
    countryCode_: {
      type: String,
      observer: 'onUpdateCountryCode_',
    },

    /** @private {!Array<!Array<!AddressComponentUI>>} */
    addressWrapper_: Object,

    /** @private */
    phoneNumber_: String,

    /** @private */
    email_: String,

    /** @private */
    canSave_: Boolean,

    /**
     * True if honorifics are enabled.
     * @private
     */
    showHonorific_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showHonorific');
      }
    }
  },

  /** @override */
  attached() {
    this.countryInfo = CountryDetailManagerImpl.getInstance();
    this.countryInfo.getCountryList().then(countryList => {
      this.countries_ = countryList;

      this.title_ =
          this.i18n(this.address.guid ? 'editAddressTitle' : 'addAddressTitle');

      // |phoneNumbers| and |emailAddresses| are a single item array.
      // See crbug.com/497934 for details.
      this.phoneNumber_ =
          this.address.phoneNumbers ? this.address.phoneNumbers[0] : '';
      this.email_ =
          this.address.emailAddresses ? this.address.emailAddresses[0] : '';

      this.async(() => {
        if (Object.keys(this.address).length === 0 && countryList.length > 0) {
          // If the address is completely empty, the dialog is creating a new
          // address. The first address in the country list is what we suspect
          // the user's country is.
          this.address.countryCode = countryList[0].countryCode;
        }
        if (this.countryCode_ === this.address.countryCode) {
          this.updateAddressWrapper_();
        } else {
          this.countryCode_ = this.address.countryCode;
        }
      });
    });

    // Open is called on the dialog after the address wrapper has been
    // updated.
  },

  /**
   * Returns a class to denote how long this entry is.
   * @param {AddressComponentUI} setting
   * @return {string}
   */
  long_(setting) {
    return setting.component.isLongField ? 'long' : '';
  },

  /**
   * Updates the wrapper that represents this address in the country's format.
   * @private
   */
  updateAddressWrapper_() {
    // Default to the last country used if no country code is provided.
    const countryCode = this.countryCode_ || this.countries_[0].countryCode;
    this.countryInfo.getAddressFormat(countryCode).then(format => {
      this.addressWrapper_ = format.components.flatMap(component => {
        // If this is the name field, add a honorific title row before the name.
        const addHonorific = component.row[0].field ===
                chrome.autofillPrivate.AddressField.FULL_NAME &&
            this.showHonorific_;
        const row = component.row.map(
            component => new AddressComponentUI(this.address, component));
        return addHonorific ?
            [[this.createHonorificAddressComponentUI(this.address)], row] :
            [row];
      });

      // Flush dom before resize and savability updates.
      flush();

      this.updateCanSave_();

      this.fire('on-update-address-wrapper');  // For easier testing.

      const dialog = /** @type {HTMLDialogElement} */ (this.$.dialog);
      if (!dialog.open) {
        dialog.showModal();
      }
    });
  },

  updateCanSave_() {
    const inputs = this.$.dialog.querySelectorAll('.address-column, select');

    for (let i = 0; i < inputs.length; ++i) {
      if (inputs[i].value) {
        this.canSave_ = true;
        this.fire('on-update-can-save');  // For easier testing.
        return;
      }
    }

    this.canSave_ = false;
    this.fire('on-update-can-save');  // For easier testing.
  },

  /**
   * @param {!chrome.autofillPrivate.CountryEntry} country
   * @return {string}
   * @private
   */
  getCode_(country) {
    return country.countryCode || 'SPACER';
  },

  /**
   * @param {!chrome.autofillPrivate.CountryEntry} country
   * @return {string}
   * @private
   */
  getName_(country) {
    return country.name || '------';
  },

  /**
   * @param {!chrome.autofillPrivate.CountryEntry} country
   * @return {boolean}
   * @private
   */
  isDivision_(country) {
    return !country.countryCode;
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.cancel();
  },

  /**
   * Handler for tapping the save button.
   * @private
   */
  onSaveButtonTap_() {
    // The Enter key can call this function even if the button is disabled.
    if (!this.canSave_) {
      return;
    }

    // Set a default country if none is set.
    if (!this.address.countryCode) {
      this.address.countryCode = this.countries_[0].countryCode;
    }

    this.address.phoneNumbers = this.phoneNumber_ ? [this.phoneNumber_] : [];
    this.address.emailAddresses = this.email_ ? [this.email_] : [];

    this.fire('save-address', this.address);
    this.$.dialog.close();
  },

  /**
   * Syncs the country code back to the address and rebuilds the address
   * wrapper for the new location.
   * @param {string|undefined} countryCode
   * @private
   */
  onUpdateCountryCode_(countryCode) {
    this.address.countryCode = countryCode;
    this.updateAddressWrapper_();
  },

  /** @private */
  onCountryChange_() {
    const countrySelect =
        /** @type {!HTMLSelectElement} */ (this.$$('select'));
    this.countryCode_ = countrySelect.value;
  },

  /**
   * Propagates focus to the <select> when country row is focused
   * (e.g. using tab navigation).
   * @private
   */
  onCountryRowFocus_() {
    const countrySelect =
        /** @type {!HTMLSelectElement} */ (this.$$('select'));
    countrySelect.focus();
  },

  /**
   * Prevents clicking random spaces within country row but outside of <select>
   * from triggering focus.
   * @param {!Event} e
   * @private
   */
  onCountryRowPointerDown_(e) {
    if (e.path[0].tagName !== 'SELECT') {
      e.preventDefault();
    }
  },

  /**
   * @param {!chrome.autofillPrivate.AddressEntry} address
   * @returns {AddressComponentUI}
   */
  createHonorificAddressComponentUI(address) {
    return new AddressComponentUI(address, {
      field: chrome.autofillPrivate.AddressField.HONORIFIC,
      fieldName: this.i18n('honorificLabel'),
      isLongField: true,
      placerholder: undefined,
    });
  },
});

/**
 * Creates a wrapper against a single data member for an address.
 */
class AddressComponentUI {
  /**
   * @param {!chrome.autofillPrivate.AddressEntry} address
   * @param {!chrome.autofillPrivate.AddressComponent} component
   */
  constructor(address, component) {
    Object.defineProperty(this, 'value', {
      get() {
        return this.getValue_();
      },
      set(newValue) {
        this.setValue_(newValue);
      },
    });
    this.address_ = address;
    this.component = component;
    this.isTextArea =
        component.field === chrome.autofillPrivate.AddressField.ADDRESS_LINES;
  }

  /**
   * Gets the value from the address that's associated with this component.
   * @return {string|undefined}
   * @private
   */
  getValue_() {
    const address = this.address_;
    switch (this.component.field) {
      case chrome.autofillPrivate.AddressField.HONORIFIC:
        return address.honorific;
      case chrome.autofillPrivate.AddressField.FULL_NAME:
        // |fullNames| is a single item array. See crbug.com/497934 for
        // details.
        return address.fullNames ? address.fullNames[0] : undefined;
      case chrome.autofillPrivate.AddressField.COMPANY_NAME:
        return address.companyName;
      case chrome.autofillPrivate.AddressField.ADDRESS_LINES:
        return address.addressLines;
      case chrome.autofillPrivate.AddressField.ADDRESS_LEVEL_1:
        return address.addressLevel1;
      case chrome.autofillPrivate.AddressField.ADDRESS_LEVEL_2:
        return address.addressLevel2;
      case chrome.autofillPrivate.AddressField.ADDRESS_LEVEL_3:
        return address.addressLevel3;
      case chrome.autofillPrivate.AddressField.POSTAL_CODE:
        return address.postalCode;
      case chrome.autofillPrivate.AddressField.SORTING_CODE:
        return address.sortingCode;
      case chrome.autofillPrivate.AddressField.COUNTRY_CODE:
        return address.countryCode;
      default:
        assertNotReached();
    }
  }

  /**
   * Sets the value in the address that's associated with this component.
   * @param {string} value
   * @private
   */
  setValue_(value) {
    const address = this.address_;
    switch (this.component.field) {
      case chrome.autofillPrivate.AddressField.HONORIFIC:
        address.honorific = value;
        break;
      case chrome.autofillPrivate.AddressField.FULL_NAME:
        address.fullNames = [value];
        break;
      case chrome.autofillPrivate.AddressField.COMPANY_NAME:
        address.companyName = value;
        break;
      case chrome.autofillPrivate.AddressField.ADDRESS_LINES:
        address.addressLines = value;
        break;
      case chrome.autofillPrivate.AddressField.ADDRESS_LEVEL_1:
        address.addressLevel1 = value;
        break;
      case chrome.autofillPrivate.AddressField.ADDRESS_LEVEL_2:
        address.addressLevel2 = value;
        break;
      case chrome.autofillPrivate.AddressField.ADDRESS_LEVEL_3:
        address.addressLevel3 = value;
        break;
      case chrome.autofillPrivate.AddressField.POSTAL_CODE:
        address.postalCode = value;
        break;
      case chrome.autofillPrivate.AddressField.SORTING_CODE:
        address.sortingCode = value;
        break;
      case chrome.autofillPrivate.AddressField.COUNTRY_CODE:
        address.countryCode = value;
        break;
      default:
        assertNotReached();
    }
  }
}

/** @interface */
class CountryDetailManager {
  /**
   * Gets the list of available countries.
   * The default country will be first, followed by a separator, followed by
   * an alphabetized list of countries available.
   * @return {!Promise<!Array<!chrome.autofillPrivate.CountryEntry>>}
   */
  getCountryList() {}

  /**
   * Gets the address format for a given country code.
   * @param {string} countryCode
   * @return {!Promise<!chrome.autofillPrivate.AddressComponents>}
   */
  getAddressFormat(countryCode) {}
}

/**
 * Default implementation. Override for testing.
 * @implements {CountryDetailManager}
 */
export class CountryDetailManagerImpl {
  /** @override */
  getCountryList() {
    return new Promise(function(callback) {
      chrome.autofillPrivate.getCountryList(callback);
    });
  }

  /** @override */
  getAddressFormat(countryCode) {
    return new Promise(function(callback) {
      chrome.autofillPrivate.getAddressComponents(countryCode, callback);
    });
  }
}

addSingletonGetter(CountryDetailManagerImpl);
