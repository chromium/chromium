// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener, removeWebUiListener, WebUiListener} from 'chrome://resources/js/cr.js';

import {findDifference} from './cups_printer_dialog_util.js';
import {PrinterListEntry, PrinterType} from './cups_printer_types.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxyImpl, CupsPrintersList} from './cups_printers_browser_proxy.js';

/**
 * Function which provides the client with metadata about a change
 * to a list of saved printers. The first parameter is the updated list of
 * printers after the change, the second parameter is the newly-added printer
 * (if it exists), and the third parameter is the newly-removed printer
 * (if it exists).
 */
type PrintersListWithDeltasCallback =
    (updatedPrinters: PrinterListEntry[], addedPrinters: PrinterListEntry[],
     removedPrinters: PrinterListEntry[]) => void;

/**
 * Function which provides the client with a list that contains the nearby
 * printers list. The parameter is the updated list of printers after any
 * changes.
 */
type PrintersListCallback = (updatedPrinters: PrinterListEntry[]) => void;

let instance: CupsPrintersEntryManager|null = null;

/**
 * Class for managing printer entries. Holds Saved, Nearby, Enterprise, Print
 * Server printers and notifies observers of any applicable changes to either
 * printer lists.
 */
export class CupsPrintersEntryManager {
  static getInstance(): CupsPrintersEntryManager {
    return instance || (instance = new CupsPrintersEntryManager());
  }

  static setInstanceForTesting(obj: CupsPrintersEntryManager): void {
    instance = obj;
  }

  static resetForTesting(): void {
    instance = null;
  }

  printServerPrinters: PrinterListEntry[];

  private enterprisePrinters_: PrinterListEntry[];
  private nearbyPrinters_: PrinterListEntry[];
  private onEnterprisePrintersChangedListener_: WebUiListener|null;
  private onEnterprisePrintersChangedListeners_: PrintersListCallback[];
  private onNearbyPrintersChangedListener_: WebUiListener|null;
  private onNearbyPrintersChangedListeners_: PrintersListCallback[];
  private onSavedPrintersChangedListeners_: PrintersListWithDeltasCallback[];
  private savedPrinters_: PrinterListEntry[];
  // Only true after the first call to setSavedPrintersList().
  private haveInitialSavedPrintersLoaded_: boolean;

  constructor() {
    this.savedPrinters_ = [];
    this.nearbyPrinters_ = [];
    this.enterprisePrinters_ = [];
    this.onSavedPrintersChangedListeners_ = [];
    this.printServerPrinters = [];
    this.onNearbyPrintersChangedListeners_ = [];
    this.onNearbyPrintersChangedListener_ = null;
    this.onEnterprisePrintersChangedListeners_ = [];
    this.onEnterprisePrintersChangedListener_ = null;
    this.haveInitialSavedPrintersLoaded_ = false;
  }

  addWebUiListeners(): void {
    // TODO(1005905): Add on-saved-printers-changed listener here once legacy
    // code is removed.
    this.onNearbyPrintersChangedListener_ = addWebUiListener(
        'on-nearby-printers-changed', this.setNearbyPrintersList.bind(this));

    this.onEnterprisePrintersChangedListener_ = addWebUiListener(
        'on-enterprise-printers-changed',
        this.onEnterprisePrintersChanged.bind(this));

    CupsPrintersBrowserProxyImpl.getInstance().startDiscoveringPrinters();
  }

  onEnterprisePrintersChanged(cupsPrintersList: CupsPrintersList): void {
    this.setEnterprisePrintersList(cupsPrintersList.printerList.map(
        printerInfo => ({printerInfo, printerType: PrinterType.ENTERPRISE})));
  }

  removeWebUiListeners(): void {
    if (this.onNearbyPrintersChangedListener_) {
      removeWebUiListener(this.onNearbyPrintersChangedListener_);
      this.onNearbyPrintersChangedListener_ = null;
    }
    if (this.onEnterprisePrintersChangedListener_) {
      removeWebUiListener(this.onEnterprisePrintersChangedListener_);
      this.onEnterprisePrintersChangedListener_ = null;
    }
  }

  get savedPrinters(): PrinterListEntry[] {
    return this.savedPrinters_;
  }

  get nearbyPrinters(): PrinterListEntry[] {
    return this.nearbyPrinters_;
  }

  get enterprisePrinters(): PrinterListEntry[] {
    return this.enterprisePrinters_;
  }

  addOnSavedPrintersChangedListener(listener: PrintersListWithDeltasCallback):
      void {
    this.onSavedPrintersChangedListeners_.push(listener);
  }

  removeOnSavedPrintersChangedListener(
      listener: PrintersListWithDeltasCallback): void {
    this.onSavedPrintersChangedListeners_ =
        this.onSavedPrintersChangedListeners_.filter(lis => lis !== listener);
  }

  addOnNearbyPrintersChangedListener(listener: PrintersListCallback): void {
    this.onNearbyPrintersChangedListeners_.push(listener);
  }

  removeOnNearbyPrintersChangedListener(listener: PrintersListCallback): void {
    this.onNearbyPrintersChangedListeners_ =
        this.onNearbyPrintersChangedListeners_.filter(lis => lis !== listener);
  }

  addOnEnterprisePrintersChangedListener(listener: PrintersListCallback): void {
    this.onEnterprisePrintersChangedListeners_.push(listener);
  }

  removeOnEnterprisePrintersChangedListener(listener: PrintersListCallback):
      void {
    this.onEnterprisePrintersChangedListeners_ =
        this.onEnterprisePrintersChangedListeners_.filter(
            lis => lis !== listener);
  }

  /**
   * Sets the saved printers list and notifies observers of any applicable
   * changes.
   */
  setSavedPrintersList(printerList: PrinterListEntry[]): void {
    let printersAdded: PrinterListEntry[] = [];
    let printersRemoved: PrinterListEntry[] = [];

    if (!this.haveInitialSavedPrintersLoaded_) {
      this.haveInitialSavedPrintersLoaded_ = true;
    } else if (printerList.length > this.savedPrinters_.length) {
      printersAdded = findDifference(printerList, this.savedPrinters_);
    } else if (printerList.length < this.savedPrinters_.length) {
      printersRemoved = findDifference(this.savedPrinters_, printerList);
    }

    this.savedPrinters_ = printerList;
    this.notifyOnSavedPrintersChangedListeners_(
        this.savedPrinters_, printersAdded, printersRemoved);
  }

  /**
   * Sets the nearby printers list and notifies observers of any applicable
   * changes.
   */
  setNearbyPrintersList(
      automaticPrinters: CupsPrinterInfo[],
      discoveredPrinters: CupsPrinterInfo[]): void {
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

  // Sets the enterprise printers list and notifies observers.
  setEnterprisePrintersList(enterprisePrinters: PrinterListEntry[]): void {
    this.enterprisePrinters_ = enterprisePrinters;
    this.notifyOnEnterprisePrintersChangedListeners_();
  }

  /**
   * Adds the found print server printers to |printServerPrinters|.
   * |foundPrinters| consist of print server printers that have not been saved
   * and will appear in the nearby printers list.
   */
  addPrintServerPrinters(foundPrinters: CupsPrintersList): void {
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
   */
  private notifyOnSavedPrintersChangedListeners_(
      savedPrinters: PrinterListEntry[], addedPrinter: PrinterListEntry[],
      removedPrinter: PrinterListEntry[]): void {
    this.onSavedPrintersChangedListeners_.forEach(
        listener => listener(savedPrinters, addedPrinter, removedPrinter));
  }

  private notifyOnNearbyPrintersChangedListeners_(): void {
    this.onNearbyPrintersChangedListeners_.forEach(
        listener => listener(this.nearbyPrinters_));
  }

  private notifyOnEnterprisePrintersChangedListeners_(): void {
    this.onEnterprisePrintersChangedListeners_.forEach(
        listener => listener(this.enterprisePrinters_));
  }
}
