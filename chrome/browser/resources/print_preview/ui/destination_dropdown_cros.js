// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
// TODO(gavinwill): Remove iron-dropdown dependency https://crbug.com/1082587.
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import './print_preview_vars_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, DestinationOrigin} from '../data/destination.js';
import {ERROR_STRING_KEY_MAP, getPrinterStatusIcon, PrinterStatusReason} from '../data/printer_status_cros.js';

Polymer({
  is: 'print-preview-destination-dropdown-cros',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!Destination} */
    value: Object,

    /** @type {!Array<!Destination>} */
    itemList: {
      type: Array,
      observer: 'enqueueDropdownRefit_',
    },

    /** @type {boolean} */
    disabled: {
      type: Boolean,
      value: false,
      observer: 'updateTabIndex_',
      reflectToAttribute: true,
    },

    driveDestinationKey: String,

    noDestinations: Boolean,

    pdfPrinterDisabled: Boolean,

    pdfDestinationKey: String,

    destinationIcon: String,

    /**
     * Index of the highlighted item in the dropdown.
     * @private
     */
    highlightedIndex_: Number,

    /** @private */
    dropdownLength_: {
      type: Number,
      computed:
          'computeDropdownLength_(itemList, pdfPrinterDisabled, driveDestinationKey, noDestinations, )',
    },

    destinationStatusText: String,
  },

  listeners: {
    'mousemove': 'onMouseMove_',
  },

  /** @override */
  attached() {
    this.updateTabIndex_();
  },

  /**
   * Enqueues a task to refit the iron-dropdown if it is open.
   * @private
   */
  enqueueDropdownRefit_() {
    const dropdown = this.$$('iron-dropdown');
    if (!this.dropdownRefitPending_ && dropdown.opened) {
      this.dropdownRefitPending_ = true;
      setTimeout(() => {
        dropdown.refit();
        this.dropdownRefitPending_ = false;
      }, 0);
    }
  },

  /** @private */
  openDropdown_() {
    if (this.disabled) {
      return;
    }

    this.highlightedIndex_ = this.getButtonListFromDropdown_().findIndex(
        item => item.value === this.value.key);
    this.$$('iron-dropdown').open();
    this.opened_ = true;
  },

  /** @private */
  closeDropdown_() {
    this.$$('iron-dropdown').close();
    this.opened_ = false;
    this.highlightedIndex_ = -1;
  },

  /**
   * Highlight the item the mouse is hovering over. If the user uses the
   * keyboard, the highlight will shift. But once the user moves the mouse,
   * the highlight should be updated based on the location of the mouse
   * cursor.
   * @param {!Event} event
   * @private
   */
  onMouseMove_(event) {
    const item = /** @type {!Element} */ (event.composedPath().find(
        elm => elm.classList && elm.classList.contains('list-item')));
    if (!item) {
      return;
    }
    this.highlightedIndex_ = this.getButtonListFromDropdown_().indexOf(item);
  },

  /** @private */
  onClick_(event) {
    const dropdown =
        /** @type {!IronDropdownElement} */ (this.$$('iron-dropdown'));
    // Exit if path includes |dropdown| because event will be handled by
    // onSelect_.
    if (event.composedPath().includes(dropdown)) {
      return;
    }

    if (dropdown.opened) {
      this.closeDropdown_();
      return;
    }
    this.openDropdown_();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSelect_(event) {
    this.dropdownValueSelected_(/** @type {!Element} */ (event.currentTarget));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onKeyDown_(event) {
    event.stopPropagation();
    const dropdown = this.$$('iron-dropdown');
    switch (event.code) {
      case 'ArrowUp':
      case 'ArrowDown':
        this.onArrowKeyPress_(event.code);
        break;
      case 'Enter': {
        if (dropdown.opened) {
          this.dropdownValueSelected_(
              this.getButtonListFromDropdown_()[this.highlightedIndex_]);
          break;
        }
        this.openDropdown_();
        break;
      }
      case 'Escape': {
        if (dropdown.opened) {
          this.closeDropdown_();
          event.preventDefault();
        }
        break;
      }
    }
  },

  /**
   * @param {string} eventCode
   * @private
   */
  onArrowKeyPress_(eventCode) {
    const dropdown = this.$$('iron-dropdown');
    const items = this.getButtonListFromDropdown_();
    if (items.length === 0) {
      return;
    }

    // If the dropdown is open, use the arrow key press to change which item is
    // highlighted in the dropdown. If the dropdown is closed, use the arrow key
    // press to change the selected destination.
    if (dropdown.opened) {
      const nextIndex = this.getNextItemIndexInList_(
          eventCode, this.highlightedIndex_, items.length);
      if (nextIndex === -1) {
        return;
      }
      this.highlightedIndex_ = nextIndex;
      items[this.highlightedIndex_].focus();
      return;
    }

    const currentIndex = items.findIndex(item => item.value === this.value.key);
    const nextIndex =
        this.getNextItemIndexInList_(eventCode, currentIndex, items.length);
    if (nextIndex === -1) {
      return;
    }
    this.fire('dropdown-value-selected', items[nextIndex]);
  },

  /**
   * @param {string} eventCode
   * @param {number} currentIndex
   * @param {number} numItems
   * @return {number} Returns -1 when the next item would be outside the list.
   * @private
   */
  getNextItemIndexInList_(eventCode, currentIndex, numItems) {
    const nextIndex =
        eventCode === 'ArrowDown' ? currentIndex + 1 : currentIndex - 1;
    return nextIndex >= 0 && nextIndex < numItems ? nextIndex : -1;
  },

  /**
   * @param {Element|undefined} dropdownItem
   * @private
   */
  dropdownValueSelected_(dropdownItem) {
    this.closeDropdown_();
    if (dropdownItem) {
      this.fire('dropdown-value-selected', dropdownItem);
    }
    this.$$('#destination-dropdown').focus();
  },

  /**
   * Returns list of all the visible items in the dropdown.
   * @return {!Array<!Element>}
   * @private
   */
  getButtonListFromDropdown_() {
    const dropdown = this.$$('iron-dropdown');
    return Array.from(dropdown.getElementsByClassName('list-item'))
        .filter(item => !item.hidden);
  },

  /**
   * Sets tabindex to -1 when dropdown is disabled to prevent the dropdown from
   * being focusable.
   * @private
   */
  updateTabIndex_() {
    this.$$('#destination-dropdown')
        .setAttribute('tabindex', this.disabled ? '-1' : '0');
  },

  /**
   * Determines if an item in the dropdown should be highlighted based on the
   * current value of |highlightedIndex_|.
   * @param {string} itemValue
   * @return {string}
   * @private
   */
  getHighlightedClass_(itemValue) {
    const itemToHighlight =
        this.getButtonListFromDropdown_()[this.highlightedIndex_];
    return itemToHighlight && itemValue === itemToHighlight.value ?
        'highlighted' :
        '';
  },

  /**
   * Close the dropdown when focus is lost except when an item in the dropdown
   * is the element that received the focus.
   * @param {!Event} event
   * @private
   */
  onBlur_(event) {
    if (!this.getButtonListFromDropdown_().includes(
            /** @type {!Element} */ (event.relatedTarget))) {
      this.closeDropdown_();
    }
  },

  /**
   * @return {number}
   * @private
   */
  computeDropdownLength_() {
    if (this.noDestinations) {
      return 1;
    }

    if (!this.itemList) {
      return 0;
    }

    // + 1 for "See more"
    let length = this.itemList.length + 1;
    if (!this.pdfPrinterDisabled) {
      length++;
    }
    if (this.driveDestinationKey) {
      length++;
    }
    return length;
  },

  /**
   * @param {!PrinterStatusReason} printerStatusReason
   * @return {string}
   * @private
   */
  getPrinterStatusErrorString_: function(printerStatusReason) {
    const errorStringKey = ERROR_STRING_KEY_MAP.get(printerStatusReason);
    return errorStringKey ? this.i18n(errorStringKey) : '';
  },

  /**
   * @param {!PrinterStatusReason} printerStatusReason
   * @param {boolean} isEnterprisePrinter
   * @return {string}
   * @private
   */
  getPrinterStatusIcon_(printerStatusReason, isEnterprisePrinter) {
    return getPrinterStatusIcon(printerStatusReason, isEnterprisePrinter);
  }
});
