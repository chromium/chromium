// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './cups_printers_entry.js';
import '../../settings_shared_css.js';

import {ListPropertyUpdateBehavior} from '//resources/js/list_property_update_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {matchesSearchTerm, sortPrinters} from './cups_printer_dialog_util.js';
import {PrinterListEntry} from './cups_printer_types.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl} from './cups_printers_browser_proxy.js';
import {CupsPrintersEntryListBehavior} from './cups_printers_entry_list_behavior.js';

// If the Show more button is visible, the minimum number of printers we show
// is 3.
const kMinVisiblePrinters = 3;

/**
 * Move a printer's position in |printerArr| from |fromIndex| to |toIndex|.
 * @param {!Array<!PrinterListEntry>} printerArr
 * @param {number} fromIndex
 * @param {number} toIndex
 */
function moveEntryInPrinters(printerArr, fromIndex, toIndex) {
  const element = printerArr[fromIndex];
  printerArr.splice(fromIndex, 1);
  printerArr.splice(toIndex, 0, element);
}

/**
 * @fileoverview 'settings-cups-enterprise-printers' is a list container for
 * Enterprise Printers.
 */
Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-cups-enterprise-printers',

  // ListPropertyUpdateBehavior is used in CupsPrintersEntryListBehavior.
  behaviors: [
    CupsPrintersEntryListBehavior,
    ListPropertyUpdateBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Search term for filtering |enterprisePrinters|.
     * @type {string}
     */
    searchTerm: {
      type: String,
      value: '',
    },

    /** @type {?CupsPrinterInfo} */
    activePrinter: {
      type: Object,
      notify: true,
    },

    /**
     * @type {number}
     * @private
     */
    activePrinterListEntryIndex_: {
      type: Number,
      value: -1,
    },

    printersCount: {
      type: Number,
      computed: 'getFilteredPrintersLength_(filteredPrinters_.*)',
      notify: true,
    },

    /**
     * List of printers filtered through a search term.
     * @type {!Array<!PrinterListEntry>}
     * @private
     */
    filteredPrinters_: {
      type: Array,
      value: () => [],
    },

    /**
     * Keeps track of whether the user has tapped the Show more button. A search
     * term will expand the collapsed list, so we need to keep track of whether
     * the list expanded because of a search term or because the user tapped on
     * the Show more button.
     * @private
     */
    hasShowMoreBeenTapped_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used by FocusRowBehavior to track the last focused element on a row.
     * @private
     */
    lastFocused_: Object,

    /**
     * Used by FocusRowBehavior to track if the list has been blurred.
     * @private
     */
    listBlurred_: Boolean,
  },

  listeners: {
    'open-action-menu': 'onOpenActionMenu_',
  },

  observers: [
    'onSearchOrPrintersChanged_(enterprisePrinters.*, searchTerm, ' +
        'hasShowMoreBeenTapped_)',
  ],

  /** @private {CupsPrintersBrowserProxy} */
  browserProxy_: null,

  /**
   * The number of printers we display if hidden printers are allowed.
   * kMinVisiblePrinters is the default value and we never show fewer printers
   * if the Show more button is visible.
   */
  visiblePrinterCounter_: kMinVisiblePrinters,

  /** @override */
  created() {
    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();
  },

  /**
   * Redoes the search whenever |searchTerm| or |enterprisePrinters| changes.
   * @private
   */
  onSearchOrPrintersChanged_() {
    if (!this.enterprisePrinters) {
      return;
    }
    // Filter printers through |searchTerm|. If |searchTerm| is empty,
    // |filteredPrinters_| is just |enterprisePrinters|.
    let updatedPrinters = this.searchTerm ?
        this.enterprisePrinters.filter(
            item => matchesSearchTerm(item.printerInfo, this.searchTerm)) :
        this.enterprisePrinters.slice();
    updatedPrinters.sort(sortPrinters);

    if (this.shouldPrinterListBeCollapsed_()) {
      // If the Show more button is visible, we only display the first
      // N < |visiblePrinterCounter_| printers and the rest are hidden.
      updatedPrinters = updatedPrinters.filter(
          (printer, idx) => idx < this.visiblePrinterCounter_);
    }

    this.updateList(
        'filteredPrinters_', printer => printer.printerInfo.printerId,
        updatedPrinters);
  },

  /** @private */
  onShowMoreTap_() {
    this.hasShowMoreBeenTapped_ = true;
  },

  /**
   * Keeps track of whether the Show more button should be visible which means
   * that the printer list is collapsed. There are two ways a collapsed list
   * may be expanded: the Show more button is tapped or if there is a search
   * term.
   * @return {boolean} True if the printer list should be collapsed.
   * @private
   */
  shouldPrinterListBeCollapsed_() {
    // If |searchTerm| is set, never collapse the list.
    if (this.searchTerm) {
      return false;
    }

    // If |hasShowMoreBeenTapped_| is set to true, never collapse the list.
    if (this.hasShowMoreBeenTapped_) {
      return false;
    }

    // If the total number of enterprise printers does not exceed the number of
    // visible printers, there is no need for the list to be collapsed.
    if (this.enterprisePrinters.length - this.visiblePrinterCounter_ < 1) {
      return false;
    }

    return true;
  },

  /**
   * @return {boolean} Returns true if the no search message should be visible.
   * @private
   */
  showNoSearchResultsMessage_() {
    return !!this.searchTerm && !this.filteredPrinters_.length;
  },

  /**
   * @return {number} Length of |filteredPrinters_|.
   * @private
   */
  getFilteredPrintersLength_() {
    return this.filteredPrinters_.length;
  },

  /**
   * @param {!CustomEvent<{target: !HTMLElement, item: !PrinterListEntry}>} e
   * @private
   */
  onOpenActionMenu_(e) {
    const item = /** @type {!PrinterListEntry} */ (e.detail.item);
    this.activePrinterListEntryIndex_ = this.enterprisePrinters.findIndex(
        printer =>
            printer.printerInfo.printerId === item.printerInfo.printerId);
    this.activePrinter =
        this.get(['enterprisePrinters', this.activePrinterListEntryIndex_])
            .printerInfo;

    const target = /** @type {!HTMLElement} */ (e.detail.target);
    this.$$('cr-action-menu').showAt(target);
  },

  /** @private */
  onViewTap_() {
    // Event is caught by 'settings-cups-printers'.
    this.fire('edit-cups-printer-details');
    this.closeActionMenu_();
  },

  /** @private */
  closeActionMenu_() {
    this.$$('cr-action-menu').close();
  },
});
