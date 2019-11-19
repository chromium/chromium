// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
   * Finds the printers that are in |firstArr| but not in |secondArr|.
   * @param {!Array<!PrinterListEntry>} firstArr
   * @param {!Array<!PrinterListEntry>} secondArr
   * @return {!Array<!PrinterListEntry>}
   * @private
   */
  function findDifference_(firstArr, secondArr) {
    return firstArr.filter((firstArrEntry) => {
      return !secondArr.some(
          p => p.printerInfo.printerId == firstArrEntry.printerInfo.printerId);
    });
  }

  /**
   * Class for managing printer entries. Holds both Saved and Nearby printers
   * and notifies observers of any applicable changes to either printer lists.
   */
  class CupsPrintersEntryManager {
    constructor() {
      /** @private {!Array<!PrinterListEntry>} */
      this.savedPrinters_ = [];

      /** @private {!Array<!PrinterListEntry>} */
      this.nearbyPrinters_ = [];

      /** @private {!Array<PrintersListWithDeltasCallback>} */
      this.onSavedPrintersChangedListeners_ = [];

      /** @type {!Array<PrintersListCallback>} */
      this.onNearbyPrintersChangedListeners_ = [];
    }

    addWebUIListeners() {
      // TODO(1005905): Add on-printers-changed listener here once legacy code
      // is removed.
      cr.addWebUIListener(
          'on-nearby-printers-changed', this.setNearbyPrintersList.bind(this));
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .startDiscoveringPrinters();
    }

    removeWebUIListeners() {
      cr.removeWebUIListener('on-nearby-printers-changed');
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
          this.onSavedPrintersChangedListeners_.filter(lis => lis != listener);
    }

    /** @param {PrintersListCallback} listener */
    addOnNearbyPrintersChangedListener(listener) {
      this.onNearbyPrintersChangedListeners_.push(listener);
    }

    /** @param {PrintersListCallback} listener */
    removeOnNearbyPrintersChangedListener(listener) {
      this.onNearbyPrintersChangedListeners_ =
          this.onNearbyPrintersChangedListeners_.filter(lis => lis != listener);
    }

    /**
     * Sets the saved printers list and notifies observers of any applicable
     * changes.
     * @param {!Array<!PrinterListEntry>} printerList
     */
    setSavedPrintersList(printerList) {
      if (printerList.length > this.savedPrinters_.length) {
        const diff = findDifference_(printerList, this.savedPrinters_);
        this.savedPrinters_ = printerList;
        this.notifyOnSavedPrintersChangedListeners_(
            this.savedPrinters_, diff, [] /* printersRemoved */);
        return;
      }

      if (printerList.length < this.savedPrinters_.length) {
        const diff = findDifference_(this.savedPrinters_, printerList);
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

  return {
    CupsPrintersEntryManager: CupsPrintersEntryManager,
  };
});