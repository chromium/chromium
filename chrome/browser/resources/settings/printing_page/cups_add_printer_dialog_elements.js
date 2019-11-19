// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** 'add-printers-list' is the list of discovered printers. */
Polymer({
  is: 'add-printer-list',

  properties: {
    /** @type {!Array<!CupsPrinterInfo>} */
    printers: {
      type: Array,
      notify: true,
    },

    /** @type {!CupsPrinterInfo} */
    selectedPrinter: {
      type: Object,
      notify: true,
    },
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onSelect_: function(event) {
    this.selectedPrinter = event.model.item;
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

/** 'add-printer-dialog' is the template of the Add Printer dialog. */
Polymer({
  is: 'add-printer-dialog',

  /** @private */
  attached: function() {
    this.$.dialog.showModal();
  },

  close: function() {
    this.$.dialog.close();
  },
});

/**
 * 'printer-dialog-error' is the error container for dialogs.
 */
Polymer({
  is: 'printer-dialog-error',

  properties: {
    /** The error text to be displayed on the dialog. */
    errorText: {
      type: String,
      value: '',
    },
  },
});
