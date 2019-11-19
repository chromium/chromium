// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-nearby-printers' is a list container for
 * Nearby Printers.
 */
Polymer({
  is: 'settings-cups-nearby-printers',

  // ListPropertyUpdateBehavior is used in CupsPrintersEntryListBehavior.
  behaviors: [
      CupsPrintersEntryListBehavior,
      ListPropertyUpdateBehavior,
      WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Search term for filtering |nearbyPrinters|.
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

    /**
     * List of printers filtered through a search term.
     * @type {!Array<!PrinterListEntry>}
     * @private
     */
    filteredPrinters_: {
      type: Array,
      value: () => [],
    },
  },

  listeners: {
    'add-automatic-printer': 'onAddAutomaticPrinter_',
  },

  observers: [
    'onSearchOrPrintersChanged_(nearbyPrinters.*, searchTerm)'
  ],

  /**
   * Redoes the search whenever |searchTerm| or |nearbyPrinters| changes.
   * @private
   */
  onSearchOrPrintersChanged_: function() {
    if (!this.nearbyPrinters) {
      return;
    }
    // Filter printers through |searchTerm|. If |searchTerm| is empty,
    // |filteredPrinters_| is just |nearbyPrinters|.
    const updatedPrinters = this.searchTerm ?
        this.nearbyPrinters.filter(
            item => settings.printing.matchesSearchTerm(
                item.printerInfo,this.searchTerm)) :
        this.nearbyPrinters.slice();

    updatedPrinters.sort(settings.printing.sortPrinters);

    this.updateList(
        'filteredPrinters_', printer => printer.printerInfo.printerId,
        updatedPrinters);
  },

  /**
   * @param {!CustomEvent<{item: !PrinterListEntry}>} e
   * @private
   */
  onAddAutomaticPrinter_: function(e) {
    const item = e.detail.item;
    this.setActivePrinter_(item);

    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .addDiscoveredPrinter(item.printerInfo.printerId)
        .then(
            this.onAddNearbyPrintersSucceeded_.bind(this,
                item.printerInfo.printerName),
            this.onAddNearbyPrinterFailed_.bind(this));
  },

  /**
   * Retrieves the index of |item| in |nearbyPrinters_| and sets that printer as
   * the active printer.
   * @param {!PrinterListEntry} item
   * @private
   */
  setActivePrinter_: function(item) {
    this.activePrinterListEntryIndex_ = this.nearbyPrinters.findIndex(
        printer => printer.printerInfo.printerId == item.printerInfo.printerId);

    this.activePrinter =
        this.get(['nearbyPrinters', this.activePrinterListEntryIndex_])
        .printerInfo;
  },

  /**
   * Handler for addDiscoveredPrinter success.
   * @param {string} printerName
   * @param {!PrinterSetupResult} result
   * @private
   */
  onAddNearbyPrintersSucceeded_: function(printerName, result) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: printerName});
  },

  /**
   * Handler for addDiscoveredPrinter failure.
   * @param {*} printer
   * @private
   */
  onAddNearbyPrinterFailed_: function(printer) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: PrinterSetupResult.PRINTER_UNREACHABLE,
         printerName: printer.printerName});
  },

  /**
   * @return {boolean} Returns true if the no search message should be visible.
   * @private
   */
  showNoSearchResultsMessage_: function() {
    return !!this.searchTerm && !this.filteredPrinters_.length;
  }
});