// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-print-server-dialog' is a dialog in which the user can
 *   add a print server.
 */

Polymer({
  is: 'add-print-server-dialog',

  properties: {
    /** @private {string} */
    printServerAddress_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    errorText_: {
      type: String,
      value: '',
    },

    /** @private {boolean} */
    inProgress_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onCancelTap_: function() {
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  onAddPrintServerTap_: function() {
    this.inProgress_ = true;
    this.$$('#printServerAddressInput').invalid = false;
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .queryPrintServer(this.printServerAddress_)
        .then(
            this.onPrintServerAddedSucceeded_.bind(this),
            this.onPrintServerAddedFailed_.bind(this));
  },

  /**
   * @param {!CupsPrintersList} printers
   * @private
   */
  onPrintServerAddedSucceeded_: function(printers) {
    this.inProgress_ = false;
    this.fire('add-print-server-and-show-toast', {printers: printers});
    this.$$('add-printer-dialog').close();
  },

  /**
   * @param {*} addPrintServerError
   * @private
   */
  onPrintServerAddedFailed_: function(addPrintServerError) {
    this.inProgress_ = false;
    if (addPrintServerError === PrintServerResult.INCORRECT_URL) {
      this.$$('#printServerAddressInput').invalid = true;
      return;
    }
    this.errorText_ = settings.printing.getPrintServerErrorText(
        /** @type {PrintServerResult} */ (addPrintServerError));
  },

  /**
   * Keypress event handler. If enter is pressed, trigger the add event.
   * @param {!Event} event
   * @private
   */
  onKeypress_: function(event) {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();

    this.onAddPrintServerTap_();
  },
});
