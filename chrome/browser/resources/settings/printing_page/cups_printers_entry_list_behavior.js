// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for observing CupsPrintersEntryManager events.
 * Use this behavior if you want to receive a dynamically updated list of both
 * saved and nearby printers.
 */

/** @polymerBehavior */
const CupsPrintersEntryListBehavior = {
  properties: {
    /** @private {!settings.printing.CupsPrintersEntryManager} */
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
  },

  /** @override */
  created: function() {
    this.entryManager_ =
        settings.printing.CupsPrintersEntryManager.getInstance();
  },

  /** @override */
  attached: function() {
    this.entryManager_.addOnSavedPrintersChangedListener(
        this.onSavedPrintersChanged_.bind(this));
    this.entryManager_.addOnNearbyPrintersChangedListener(
        this.onNearbyPrintersChanged_.bind(this));

    // Initialize saved and nearby printers list.
    this.onSavedPrintersChanged_(
        this.entryManager_.savedPrinters, [] /* printerAdded */,
        [] /* printerRemoved */);
    this.onNearbyPrintersChanged_(this.entryManager_.nearbyPrinters);
  },

  /** @override */
  detached: function() {
    this.entryManager_.removeOnSavedPrintersChangedListener(
        this.onSavedPrintersChanged_.bind(this));
    this.entryManager_.removeOnNearbyPrintersChangedListener(
        this.onNearbyPrintersChanged_.bind(this));
  },

  /**
   * Non-empty params indicate the applicable change to be notified.
   * @param {!Array<!PrinterListEntry>} savedPrinters
   * @param {!Array<!PrinterListEntry>} addedPrinters
   * @param {!Array<!PrinterListEntry>} removedPrinters
   * @private
   */
  onSavedPrintersChanged_: function(
      savedPrinters, addedPrinters, removedPrinters) {
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
  onNearbyPrintersChanged_: function(printerList) {
    this.updateList(
        'nearbyPrinters', printer => printer.printerInfo.printerId,
        printerList);
  },

  // CupsPrintersEntryListBehavior methods. Override these in the
  // implementations.

  /** @param{!Array<!PrinterListEntry>} addedPrinters */
  onSavedPrintersAdded: function(addedPrinters) {},

  /** @param{!Array<!PrinterListEntry>} removedPrinters */
  onSavedPrintersRemoved: function(removedPrinters) {},
};