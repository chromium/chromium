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

    printersCount: {
      type: Number,
      computed: 'getFilteredPrintersLength_(filteredPrinters_.*)',
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
    'add-print-server-printer': 'onAddPrintServerPrinter_',
    'query-discovered-printer': 'onQueryDiscoveredPrinter_',
  },

  observers: ['onSearchOrPrintersChanged_(nearbyPrinters.*, searchTerm)'],

  /**
   * Redoes the search whenever |searchTerm| or |nearbyPrinters| changes.
   * @private
   */
  onSearchOrPrintersChanged_() {
    if (!this.nearbyPrinters) {
      return;
    }
    // Filter printers through |searchTerm|. If |searchTerm| is empty,
    // |filteredPrinters_| is just |nearbyPrinters|.
    const updatedPrinters = this.searchTerm ?
        this.nearbyPrinters.filter(
            item => settings.printing.matchesSearchTerm(
                item.printerInfo, this.searchTerm)) :
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
  onAddAutomaticPrinter_(e) {
    const item = e.detail.item;
    this.setActivePrinter_(item);

    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .addDiscoveredPrinter(item.printerInfo.printerId)
        .then(
            this.onAddNearbyPrintersSucceeded_.bind(
                this, item.printerInfo.printerName),
            this.onAddNearbyPrinterFailed_.bind(this));
    settings.recordSettingChange();
  },

  /**
   * @param {!CustomEvent<{item: !PrinterListEntry}>} e
   * @private
   */
  onAddPrintServerPrinter_(e) {
    const item = e.detail.item;
    this.setActivePrinter_(item);

    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .addCupsPrinter(item.printerInfo)
        .then(
            this.onAddNearbyPrintersSucceeded_.bind(
                this, item.printerInfo.printerName),
            this.onAddNearbyPrinterFailed_.bind(this));
  },

  /**
   * @param {!CustomEvent<{item: !PrinterListEntry}>} e
   * @private
   */
  onQueryDiscoveredPrinter_(e) {
    const item = e.detail.item;
    this.setActivePrinter_(item);

    // This is a workaround to ensure type safety on the params of the casted
    // function. We do this because the closure compiler does not work well with
    // rejected js promises.
    const queryDiscoveredPrinterFailed = /** @type {!Function}) */ (
        this.onQueryDiscoveredPrinterFailed_.bind(this));
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .addDiscoveredPrinter(item.printerInfo.printerId)
        .then(
            this.onQueryDiscoveredPrinterSucceeded_.bind(
                this, item.printerInfo.printerName),
            queryDiscoveredPrinterFailed);
    settings.recordSettingChange();
  },

  /**
   * Retrieves the index of |item| in |nearbyPrinters_| and sets that printer as
   * the active printer.
   * @param {!PrinterListEntry} item
   * @private
   */
  setActivePrinter_(item) {
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
  onAddNearbyPrintersSucceeded_(printerName, result) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: printerName});
  },

  /**
   * Handler for addDiscoveredPrinter failure.
   * @param {*} printer
   * @private
   */
  onAddNearbyPrinterFailed_(printer) {
    this.fire('show-cups-printer-toast', {
      resultCode: PrinterSetupResult.PRINTER_UNREACHABLE,
      printerName: printer.printerName
    });
  },

  /**
   * Handler for queryDiscoveredPrinter success.
   * @param {string} printerName
   * @param {!PrinterSetupResult} result
   * @private
   */
  onQueryDiscoveredPrinterSucceeded_(printerName, result) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: printerName});
  },

  /**
   * Handler for queryDiscoveredPrinter failure.
   * @param {!CupsPrinterInfo} printer
   * @private
   */
  onQueryDiscoveredPrinterFailed_(printer) {
    this.fire(
        'open-manufacturer-model-dialog-for-specified-printer',
        {item: /** @type {CupsPrinterInfo} */ (printer)});
  },

  /**
   * @return {boolean} Returns true if the no search message should be visible.
   * @private
   */
  showNoSearchResultsMessage_() {
    return !!this.searchTerm && !this.filteredPrinters_.length;
  },

  /**
   * @private
   * @return {number} Length of |filteredPrinters_|.
   */
  getFilteredPrintersLength_() {
    return this.filteredPrinters_.length;
  },
});
