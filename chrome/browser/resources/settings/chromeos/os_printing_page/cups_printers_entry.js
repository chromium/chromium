// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers-entry' is a component that holds a
 * printer.
 */
Polymer({
  is: 'settings-cups-printers-entry',

  properties: {
    /** @type {!PrinterListEntry} */
    printerEntry: Object,

    /**
     * TODO(jimmyxgong): Determine how subtext should be set and what
     * information it should have, including necessary ARIA labeling
     * The additional information subtext for a printer.
     * @type {string}
     */
    subtext: {type: String, value: ''},
  },

  /**
   * Fires a custom event when the menu button is clicked. Sends the details of
   * the printer and where the menu should appear.
   */
  onOpenActionMenuTap_(e) {
    this.fire('open-action-menu', {
      target: e.target,
      item: this.printerEntry,
    });
  },

  /** @private */
  onAddDiscoveredPrinterTap_(e) {
    this.fire('query-discovered-printer', {item: this.printerEntry});
  },

  /** @private */
  onAddAutomaticPrinterTap_() {
    this.fire('add-automatic-printer', {item: this.printerEntry});
  },

  /** @private */
  onAddServerPrinterTap_: function() {
    this.fire('add-print-server-printer', {item: this.printerEntry});
  },

  /**
   * @return {boolean}
   * @private
   */
  isSavedPrinter_() {
    return this.printerEntry.printerType == PrinterType.SAVED;
  },

  /**
   * @return {boolean}
   * @private
   */
  isDiscoveredPrinter_() {
    return this.printerEntry.printerType == PrinterType.DISCOVERED;
  },

  /**
   * @return {boolean}
   * @private
   */
  isAutomaticPrinter_() {
    return this.printerEntry.printerType == PrinterType.AUTOMATIC;
  },

  /**
   * @return {boolean}
   * @private
   */
  isPrintServerPrinter_() {
    return this.printerEntry.printerType == PrinterType.PRINTSERVER;
  },

  getSaveButtonAria_() {
    return loadTimeData.getStringF(
        'savePrinterAria', this.printerEntry.printerInfo.printerName);
  },

  getSetupButtonAria_() {
    return loadTimeData.getStringF(
        'setupPrinterAria', this.printerEntry.printerInfo.printerName);
  },
});
