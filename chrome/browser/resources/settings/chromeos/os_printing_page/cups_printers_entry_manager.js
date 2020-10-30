// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addWebUIListener, addSingletonGetter, removeWebUIListener} from 'chrome://resources/js/cr.m.js';
// #import {WebUIListener} from 'chrome://resources/js/cr.m.js';
// #import {CupsPrintersBrowserProxyImpl, CupsPrintersList, CupsPrinterInfo} from './cups_printers_browser_proxy.m.js';
// #import {findDifference} from './cups_printer_dialog_util.m.js';
// #import {PrinterType, PrinterListEntry} from './cups_printer_types.m.js';
// clang-format on

/**
 * Function which provides the client with metadata about a change
 * to a list of saved printers. The first parameter is the updated list of
 * printers after the change, the second parameter is the newly-added printer
 * (if it exists), and the third parameter is the newly-removed printer
 * (if it exists).
 * @typedef {!function(!Array<!PrinterListEntry>, !Array<!PrinterListEntry>,
 *     !Array<!PrinterListEntry>): void}
 */
let PrintersListWithDeltasCallback;

/**
 * Function which provides the client with a list that contains the nearby
 * printers list. The parameter is the updated list of printers after any
 * changes.
 * @typedef {function(!Array<!PrinterListEntry>): void}
 */
let PrintersListCallback;

cr.define('settings.printing', function() {
  /**
   * Class for managing printer entries. Holds both Saved, Nearby, Print Server
   * printers and notifies observers of any applicable changes to either printer
   * lists.
   */
  /* #export */ class CupsPrintersEntryManager {
    constructor() {
      /** @private {!Array<!PrinterListEntry>} */
      this.savedPrinters_ = [];

      /** @private {!Array<!PrinterListEntry>} */
      this.nearbyPrinters_ = [];

      /** @private {!Array<PrintersListWithDeltasCallback>} */
      this.onSavedPrintersChangedListeners_ = [];

      /** @type {!Array<!PrinterListEntry>} */
      this.printServerPrinters = [];

      /** @private {!Array<PrintersListCallback>} */
      this.onNearbyPrintersChangedListeners_ = [];

      /** @private {?WebUIListener} */
      this.onNearbyPrintersChangedListener_ = null;
    }

    addWebUIListeners() {
      // TODO(1005905): Add on-printers-changed listener here once legacy code
      // is removed.
      this.onNearbyPrintersChangedListener_ = cr.addWebUIListener(
          'on-nearby-printers-changed', this.setNearbyPrintersList.bind(this));

      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .startDiscoveringPrinters();
    }

    removeWebUIListeners() {
      if (this.onNearbyPrintersChangedListener_) {
        cr.removeWebUIListener(/** @type {WebUIListener} */ (
            this.onNearbyPrintersChangedListener_));
        this.onNearbyPrintersChangedListener_ = null;
      }
    }

    /** @return {!Array<!PrinterListEntry>} */
    get savedPrinters() {
      return this.savedPrinters_;
    }

    /** @return {!Array<!PrinterListEntry>} */
    get nearbyPrinters() {
      return this.nearbyPrinters_;
    }

    /** @param {PrintersListWithDeltasCallback} listener */
    addOnSavedPrintersChangedListener(listener) {
      this.onSavedPrintersChangedListeners_.push(listener);
    }

    /** @param {PrintersListWithDeltasCallback} listener */
    removeOnSavedPrintersChangedListener(listener) {
      this.onSavedPrintersChangedListeners_ =
          this.onSavedPrintersChangedListeners_.filter(lis => lis !== listener);
    }

    /** @param {PrintersListCallback} listener */
    addOnNearbyPrintersChangedListener(listener) {
      this.onNearbyPrintersChangedListeners_.push(listener);
    }

    /** @param {PrintersListCallback} listener */
    removeOnNearbyPrintersChangedListener(listener) {
      this.onNearbyPrintersChangedListeners_ =
          this.onNearbyPrintersChangedListeners_.filter(
              lis => lis !== listener);
    }

    /**
     * Sets the saved printers list and notifies observers of any applicable
     * changes.
     * @param {!Array<!PrinterListEntry>} printerList
     */
    setSavedPrintersList(printerList) {
      if (printerList.length > this.savedPrinters_.length) {
        const diff =
            settings.printing.findDifference(printerList, this.savedPrinters_);
        this.savedPrinters_ = printerList;
        this.notifyOnSavedPrintersChangedListeners_(
            this.savedPrinters_, diff, [] /* printersRemoved */);
        return;
      }

      if (printerList.length < this.savedPrinters_.length) {
        const diff =
            settings.printing.findDifference(this.savedPrinters_, printerList);
        this.savedPrinters_ = printerList;
        this.notifyOnSavedPrintersChangedListeners_(
            this.savedPrinters_, [] /* printersAdded */, diff);
        return;
      }

      this.savedPrinters_ = printerList;
      this.notifyOnSavedPrintersChangedListeners_(
          this.savedPrinters_, [] /* printersAdded */,
          [] /* printersRemoved */);
    }

    /**
     * Sets the nearby printers list and notifies observers of any applicable
     * changes.
     * @param {!Array<!CupsPrinterInfo>} automaticPrinters
     * @param {!Array<!CupsPrinterInfo>} discoveredPrinters
     */
    setNearbyPrintersList(automaticPrinters, discoveredPrinters) {
      if (!automaticPrinters && !discoveredPrinters) {
        return;
      }

      this.nearbyPrinters_ = [];

      for (const printer of automaticPrinters) {
        this.nearbyPrinters_.push(
            {printerInfo: printer, printerType: PrinterType.AUTOMATIC});
      }

      for (const printer of discoveredPrinters) {
        this.nearbyPrinters_.push(
            {printerInfo: printer, printerType: PrinterType.DISCOVERED});
      }

      this.notifyOnNearbyPrintersChangedListeners_();
    }

    /**
     * Adds the found print server printers to |printServerPrinters|.
     * |foundPrinters| consist of print server printers that have not been saved
     * and will appear in the nearby printers list.
     * @param {!CupsPrintersList} foundPrinters
     */
    addPrintServerPrinters(foundPrinters) {
      // Get only new printers from |foundPrinters|. We ignore previously
      // found printers.
      const newPrinters = foundPrinters.printerList.filter(p1 => {
        return !this.printServerPrinters.some(
            p2 => p2.printerInfo.printerId === p1.printerId);
      });

      for (const printer of newPrinters) {
        this.printServerPrinters.push(
            {printerInfo: printer, printerType: PrinterType.PRINTSERVER});
      }

      // All printers from print servers are treated as nearby printers.
      this.notifyOnNearbyPrintersChangedListeners_();
    }

    /**
     * Non-empty/null fields indicate the applicable change to be notified.
     * @param {!Array<!PrinterListEntry>} savedPrinters
     * @param {!Array<!PrinterListEntry>} addedPrinter
     * @param {!Array<!PrinterListEntry>} removedPrinter
     * @private
     */
    notifyOnSavedPrintersChangedListeners_(
        savedPrinters, addedPrinter, removedPrinter) {
      this.onSavedPrintersChangedListeners_.forEach(
          listener => listener(savedPrinters, addedPrinter, removedPrinter));
    }

    /** @private */
    notifyOnNearbyPrintersChangedListeners_() {
      this.onNearbyPrintersChangedListeners_.forEach(
          listener => listener(this.nearbyPrinters_));
    }
  }

  cr.addSingletonGetter(CupsPrintersEntryManager);

  // #cr_define_end
  return {
    CupsPrintersEntryManager: CupsPrintersEntryManager,
  };
});
