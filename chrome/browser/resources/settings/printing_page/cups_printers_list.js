// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers-list' is a component for a list of
 * CUPS printers.
 */
Polymer({
  is: 'settings-cups-printers-list',

  properties: {
    /** @type {!Array<!CupsPrinterInfo>} */
    printers: {
      type: Array,
      notify: true,
    },

    searchTerm: {
      type: String,
    },

    /**
     * The model for the printer action menu.
     * @type {?CupsPrinterInfo}
     */
    activePrinter: {
      type: Object,
      notify: true,
    },
  },

  /** @private {settings.CupsPrintersBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.CupsPrintersBrowserProxyImpl.getInstance();
  },

  /**
   * @param {!{model: !{item: !CupsPrinterInfo}}} e
   * @private
   */
  onOpenActionMenuTap_: function(e) {
    this.activePrinter = e.model.item;
    const menu =
        /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'));
    menu.showAt(/** @type {!Element} */ (/** @type {!Event} */ (e).target));
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onEditTap_: function(event) {
    // Event is caught by 'settings-cups-printers'.
    this.fire('edit-cups-printer-details');
    this.closeDropdownMenu_();
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onRemoveTap_: function(event) {
    const index = this.printers.indexOf(assert(this.activePrinter));
    this.splice('printers', index, 1);
    this.browserProxy_.removeCupsPrinter(
        this.activePrinter.printerId, this.activePrinter.printerName);
    this.activePrinter = null;
    this.closeDropdownMenu_();
  },

  /** @private */
  closeDropdownMenu_: function() {
    const menu =
        /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'));
    menu.close();
  },

  /**
   * The filter callback function to show printers based on |searchTerm|.
   * @param {string} searchTerm
   * @private
   */
  filterPrinter_: function(searchTerm) {
    if (!searchTerm) {
      return null;
    }
    return function(printer) {
      return printer.printerName.toLowerCase().includes(
          searchTerm.toLowerCase());
    };
  },

  /**
   * @param {!CupsPrinterInfo} first
   * @param {!CupsPrinterInfo} second
   * @return {number} The result of the comparison.
   * @private
   */
  sort_: function(first, second) {
    return settings.printing.alphabeticalSort(first, second);
  },
});
