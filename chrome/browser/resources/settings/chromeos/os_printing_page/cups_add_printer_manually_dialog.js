// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-printer-manually-dialog' is a dialog in which user can
 * manually enter the information to set up a new printer.
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
  is: 'add-printer-manually-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {type: Object, notify: true, value: getEmptyPrinter_},

    /** @private */
    addPrinterInProgress_: {
      type: Boolean,
      value: false,
    },

    /**
     * The error text to be displayed on the dialog.
     * @private
     */
    errorText_: {
      type: String,
      value: '',
    },
  },

  observers: [
    'printerInfoChanged_(newPrinter.*)',
  ],

  /** @private */
  onCancelTap_() {
    this.$$('add-printer-dialog').close();
  },

  /**
   * Handler for addCupsPrinter success.
   * @param {!PrinterSetupResult} result
   * @private
   */
  onAddPrinterSucceeded_(result) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: this.newPrinter.printerName});
    this.$$('add-printer-dialog').close();
  },

  /**
   * Handler for addCupsPrinter failure.
   * @param {*} result
   * @private
   */
  onAddPrinterFailed_(result) {
    this.errorText_ = settings.printing.getErrorText(
        /** @type {PrinterSetupResult} */ (result));
  },

  /**
   * Handler for getPrinterInfo success.
   * @param {!PrinterMakeModel} info
   * @private
   */
  onPrinterFound_(info) {
    const newPrinter =
        /** @type {CupsPrinterInfo}  */ (Object.assign({}, this.newPrinter));

    newPrinter.printerManufacturer = info.manufacturer;
    newPrinter.printerModel = info.model;
    newPrinter.printerMakeAndModel = info.makeAndModel;
    newPrinter.printerPpdReference.userSuppliedPpdUrl =
        info.ppdRefUserSuppliedPpdUrl;
    newPrinter.printerPpdReference.effectiveMakeAndModel =
        info.ppdRefEffectiveMakeAndModel;
    newPrinter.printerPpdReference.autoconf = info.autoconf;

    this.newPrinter = newPrinter;


    // Add the printer if it's configurable. Otherwise, forward to the
    // manufacturer dialog.
    if (info.ppdReferenceResolved) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .addCupsPrinter(this.newPrinter)
          .then(
              this.onAddPrinterSucceeded_.bind(this),
              this.onAddPrinterFailed_.bind(this));
    } else {
      this.$$('add-printer-dialog').close();
      this.fire('open-manufacturer-model-dialog');
    }
  },

  /**
   * Handler for getPrinterInfo failure.
   * @param {*} result a PrinterSetupResult with an error code indicating why
   * getPrinterInfo failed.
   * @private
   */
  infoFailed_(result) {
    this.addPrinterInProgress_ = false;
    if (result == PrinterSetupResult.PRINTER_UNREACHABLE) {
      this.$.printerAddressInput.invalid = true;
      return;
    }
    this.errorText_ = settings.printing.getErrorText(
        /** @type {PrinterSetupResult} */ (result));
  },

  /** @private */
  addPressed_() {
    this.addPrinterInProgress_ = true;

    if (this.newPrinter.printerProtocol == 'ipp' ||
        this.newPrinter.printerProtocol == 'ipps') {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getPrinterInfo(this.newPrinter)
          .then(this.onPrinterFound_.bind(this), this.infoFailed_.bind(this));
    } else {
      this.$$('add-printer-dialog').close();
      this.fire('open-manufacturer-model-dialog');
    }
  },

  /** @private */
  onPrintServerTap_: function() {
    this.$$('add-printer-dialog').close();
    this.fire('open-add-print-server-dialog');
  },

  /**
   * @param {!Event} event
   * @private
   */
  onProtocolChange_(event) {
    this.set('newPrinter.printerProtocol', event.target.value);
  },

  /**
   * @return {boolean} Whether the add printer button is enabled.
   * @private
   */
  canAddPrinter_() {
    return !this.addPrinterInProgress_ &&
        settings.printing.isNameAndAddressValid(this.newPrinter);
  },

  /** @private */
  printerInfoChanged_() {
    this.$.printerAddressInput.invalid = false;
    this.errorText_ = '';
  },

  /**
   * Keypress event handler. If enter is pressed, printer is added if
   * |canAddPrinter_| is true.
   * @param {!Event} event
   * @private
   */
  onKeypress_(event) {
    if (event.key != 'Enter') {
      return;
    }
    event.stopPropagation();

    if (this.canAddPrinter_()) {
      this.addPressed_();
    }
  },
});