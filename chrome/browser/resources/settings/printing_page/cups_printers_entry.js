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
     * information it should have.
     * The additional information subtext for a printer.
     * @type {string}
     */
    subtext: {type: String, value: ''},
  },

  /**
   * Fires a custom event when the menu button is clicked. Sends the details of
   * the printer and where the menu should appear.
   */
  onOpenActionMenuTap_: function(e) {
    this.fire('open-action-menu', {
      target: e.target,
      item: this.printerEntry,
    });
  },

  onOpenManufacturerModelDialogTap_: function(e) {
    this.fire('open-manufacturer-model-dialog-for-specified-printer',
        {item: this.printerEntry.printerInfo});
  },

  onAddAutomaticPrinterTap_: function() {
    this.fire('add-automatic-printer', {item: this.printerEntry});
  },

  /**
   * @return {boolean}
   * @private
   */
  isSavedPrinter_: function() {
    return this.printerEntry.printerType == PrinterType.SAVED;
  },

  /**
   * @return {boolean}
   * @private
   */
  isDiscoveredPrinter_: function() {
    return this.printerEntry.printerType == PrinterType.DISCOVERED;
  },

  /**
   * @return {boolean}
   * @private
   */
  isAutomaticPrinter_: function() {
    return this.printerEntry.printerType == PrinterType.AUTOMATIC;
  },

  getSaveButtonAria_: function() {
    return loadTimeData.getStringF('savePrinterAria',
      this.printerEntry.printerInfo.printerName);
  },

  getSetupButtonAria_: function() {
    return loadTimeData.getStringF('setupPrinterAria',
      this.printerEntry.printerInfo.printerName);
  },
});
