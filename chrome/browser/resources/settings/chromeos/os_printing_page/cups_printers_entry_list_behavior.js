// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for observing CupsPrintersEntryManager events.
 * Use this behavior if you want to receive a dynamically updated list of both
 * saved and nearby printers.
 */

import {assert} from 'chrome://resources/ash/common/assert.js';

import {findDifference} from './cups_printer_dialog_util.js';
import {PrinterListEntry} from './cups_printer_types.js';
import {CupsPrintersEntryManager} from './cups_printers_entry_manager.js';

/** @polymerBehavior */
export const CupsPrintersEntryListBehavior = {
  properties: {
    /** @private {!CupsPrintersEntryManager} */
    entryManager_: Object,

    /** @type {!Array<!PrinterListEntry>} */
    savedPrinters: {
      type: Array,
      value: () => [],
    },

    /** @type {!Array<!PrinterListEntry>} */
    nearbyPrinters: {
      type: Array,
      value: () => [],
    },

    /** @type {!Array<!PrinterListEntry>} */
    enterprisePrinters: {
      type: Array,
      value: () => [],
    },
  },

  /** @override */
  created() {
    this.entryManager_ = CupsPrintersEntryManager.getInstance();
  },

  /** @override */
  attached() {
    this.entryManager_.addOnSavedPrintersChangedListener(
        this.onSavedPrintersChanged_.bind(this));
    this.entryManager_.addOnNearbyPrintersChangedListener(
        this.onNearbyPrintersChanged_.bind(this));
    this.entryManager_.addOnEnterprisePrintersChangedListener(
        this.onEnterprisePrintersChanged_.bind(this));

    // Initialize saved and nearby printers list.
    this.onSavedPrintersChanged_(
        this.entryManager_.savedPrinters, [] /* printerAdded */,
        [] /* printerRemoved */);
    this.onNearbyPrintersChanged_(this.entryManager_.nearbyPrinters);
    this.onEnterprisePrintersChanged_(this.entryManager_.enterprisePrinters);
  },

  /** @override */
  detached() {
    this.entryManager_.removeOnSavedPrintersChangedListener(
        this.onSavedPrintersChanged_.bind(this));
    this.entryManager_.removeOnNearbyPrintersChangedListener(
        this.onNearbyPrintersChanged_.bind(this));
    this.entryManager_.removeOnEnterprisePrintersChangedListener(
        this.onEnterprisePrintersChanged_.bind(this));
  },

  /**
   * Non-empty params indicate the applicable change to be notified.
   * @param {!Array<!PrinterListEntry>} savedPrinters
   * @param {!Array<!PrinterListEntry>} addedPrinters
   * @param {!Array<!PrinterListEntry>} removedPrinters
   * @private
   */
  onSavedPrintersChanged_(savedPrinters, addedPrinters, removedPrinters) {
    this.updateList(
        'savedPrinters', printer => printer.printerInfo.printerId,
        savedPrinters);

    assert(!(addedPrinters.length && removedPrinters.length));

    if (addedPrinters.length) {
      this.onSavedPrintersAdded(addedPrinters);
    } else if (removedPrinters.length) {
      this.onSavedPrintersRemoved(removedPrinters);
    }
  },

  /**
   * @param {!Array<!PrinterListEntry>} printerList
   * @private
   */
  onNearbyPrintersChanged_(printerList) {
    // |printerList| consists of automatic and discovered printers that have
    // not been saved and are available. Add all unsaved print server printers
    // to |printerList|.
    this.entryManager_.printServerPrinters = findDifference(
        this.entryManager_.printServerPrinters, this.savedPrinters);
    printerList = printerList.concat(this.entryManager_.printServerPrinters);

    this.updateList(
        'nearbyPrinters', printer => printer.printerInfo.printerId,
        printerList);
  },

  /**
   * @param {!Array<!PrinterListEntry>} enterprisePrinters
   * @private
   */
  onEnterprisePrintersChanged_(enterprisePrinters) {
    this.updateList(
        'enterprisePrinters', printer => printer.printerInfo.printerId,
        enterprisePrinters);
  },

  // CupsPrintersEntryListBehavior methods. Override these in the
  // implementations.

  /** @param{!Array<!PrinterListEntry>} addedPrinters */
  onSavedPrintersAdded(addedPrinters) {},

  /** @param{!Array<!PrinterListEntry>} removedPrinters */
  onSavedPrintersRemoved(removedPrinters) {},
};

/** @interface */
export class CupsPrintersEntryListBehaviorInterface {
  constructor() {
    /** @type {!Array<!PrinterListEntry>} */
    this.savedPrinters;

    /** @type {!Array<!PrinterListEntry>} */
    this.nearbyPrinters;

    /** @type {!Array<!PrinterListEntry>} */
    this.enterprisePrinters;
  }

  /** @param {!Array<!PrinterListEntry>} addedPrinters */
  onSavedPrintersAdded(addedPrinters) {}

  /** @param {!Array<!PrinterListEntry>} removedPrinters */
  onSavedPrintersRemoved(removedPrinters) {}
}
