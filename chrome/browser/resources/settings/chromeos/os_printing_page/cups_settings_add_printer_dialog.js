// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-add-printer-dialog' includes multiple dialogs to
 * set up a new CUPS printer.
 * Subdialogs include:
 * - 'add-printer-manually-dialog' is a dialog in which user can manually enter
 *   the information to set up a new printer.
 * - 'add-printer-manufacturer-model-dialog' is a dialog in which the user can
 *   manually select the manufacture and model of the new printer.
 * - 'add-print-server-dialog' is a dialog in which the user can
 *   add a print server.
 */

/**
 * Different dialogs in add printer flow.
 * @enum {string}
 */
const AddPrinterDialogs = {
  MANUALLY: 'add-printer-manually-dialog',
  MANUFACTURER: 'add-printer-manufacturer-model-dialog',
  PRINTSERVER: 'add-print-server-dialog',
};

/**
 * Return a reset CupsPrinterInfo object.
 *  @return {!CupsPrinterInfo}
 */
function getEmptyPrinter_() {
  return {
    ppdManufacturer: '',
    ppdModel: '',
    printerAddress: '',
    printerDescription: '',
    printerId: '',
    printerManufacturer: '',
    printerModel: '',
    printerMakeAndModel: '',
    printerName: '',
    printerPPDPath: '',
    printerPpdReference: {
      userSuppliedPpdUrl: '',
      effectiveMakeAndModel: '',
      autoconf: false,
    },
    printerProtocol: 'ipp',
    printerQueue: 'ipp/print',
    printerStatus: '',
    printServerUri: '',
  };
}


Polymer({
  is: 'settings-cups-add-printer-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {
      type: Object,
    },

    /** @private {string} */
    previousDialog_: String,

    /** @private {string} */
    currentDialog_: String,

    /** @private {boolean} */
    showManuallyAddDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showManufacturerDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showAddPrintServerDialog_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'open-manually-add-printer-dialog': 'openManuallyAddPrinterDialog_',
    'open-manufacturer-model-dialog':
        'openManufacturerModelDialogForCurrentPrinter_',
    'open-add-print-server-dialog': 'openPrintServerDialog_',
    'no-detected-printer': 'onNoDetectedPrinter_',
  },

  /** Opens the Add manual printer dialog. */
  open() {
    this.resetData_();
    this.switchDialog_(
        '', AddPrinterDialogs.MANUALLY, 'showManuallyAddDialog_');
  },

  /**
   * Reset all the printer data in the Add printer flow.
   * @private
   */
  resetData_() {
    if (this.newPrinter) {
      this.newPrinter = getEmptyPrinter_();
    }
  },

  /** @private */
  openManuallyAddPrinterDialog_() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUALLY,
        'showManuallyAddDialog_');
  },

  /** @private */
  openManufacturerModelDialogForCurrentPrinter_() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUFACTURER,
        'showManufacturerDialog_');
  },

  /** @param {!CupsPrinterInfo} printer */
  openManufacturerModelDialogForSpecifiedPrinter(printer) {
    this.newPrinter = printer;
    this.switchDialog_(
        '', AddPrinterDialogs.MANUFACTURER, 'showManufacturerDialog_');
  },

  /** @private */
  openPrintServerDialog_: function() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.PRINTSERVER,
        'showAddPrintServerDialog_');
  },

  /**
   * Switch dialog from |fromDialog| to |toDialog|.
   * @param {string} fromDialog
   * @param {string} toDialog
   * @param {string} domIfBooleanName The name of the boolean variable
   *     corresponding to the |toDialog|.
   * @private
   */
  switchDialog_(fromDialog, toDialog, domIfBooleanName) {
    this.previousDialog_ = fromDialog;
    this.currentDialog_ = toDialog;

    this.set(domIfBooleanName, true);
    this.async(function() {
      const dialog = this.$$(toDialog);
      dialog.addEventListener('close', () => {
        this.set(domIfBooleanName, false);
      });
    });
  },
});
