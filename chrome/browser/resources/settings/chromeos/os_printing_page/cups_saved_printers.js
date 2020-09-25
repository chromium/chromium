// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

// If the Show more button is visible, the minimum number of printers we show
// is 3.
const kMinVisiblePrinters = 3;

/**
 * Move a printer's position in |printerArr| from |fromIndex| to |toIndex|.
 * @param {!Array<!PrinterListEntry>} printerArr
 * @param {number} fromIndex
 * @param {number} toIndex
 */
function moveEntryInPrinters(printerArr, fromIndex, toIndex) {
  const element = printerArr[fromIndex];
  printerArr.splice(fromIndex, 1);
  printerArr.splice(toIndex, 0, element);
}

/**
 * @fileoverview 'settings-cups-saved-printers' is a list container for Saved
 * Printers.
 */
Polymer({
  is: 'settings-cups-saved-printers',

  // ListPropertyUpdateBehavior is used in CupsPrintersEntryListBehavior.
  behaviors: [
    CupsPrintersEntryListBehavior,
    ListPropertyUpdateBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Search term for filtering |savedPrinters|.
     * @type {string}
     */
    searchTerm: {
      type: String,
      value: '',
    },

    /** @type {?CupsPrinterInfo} */
    activePrinter: {
      type: Object,
      notify: true,
    },

    printersCount: {
      type: Number,
      computed: 'getFilteredPrintersLength_(filteredPrinters_.*)',
      notify: true,
    },

    /**
     * @type {number}
     * @private
     */
    activePrinterListEntryIndex_: {
      type: Number,
      value: -1,
    },

    /**
     * List of printers filtered through a search term.
     * @type {!Array<!PrinterListEntry>}
     * @private
     */
    filteredPrinters_: {
      type: Array,
      value: () => [],
    },

    /**
     * Array of new PrinterListEntry's that were added during this session.
     * @type {!Array<!PrinterListEntry>}
     * @private
     */
    newPrinters_: {
      type: Array,
      value: () => [],
    },

    /**
     * Keeps track of whether the user has tapped the Show more button. A search
     * term will expand the collapsed list, so we need to keep track of whether
     * the list expanded because of a search term or because the user tapped on
     * the Show more button.
     * @private
     */
    hasShowMoreBeenTapped_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'open-action-menu': 'onOpenActionMenu_',
  },

  observers:
      ['onSearchOrPrintersChanged_(savedPrinters.*, searchTerm,' +
       'hasShowMoreBeenTapped_, newPrinters_.*)'],

  /** @private {settings.CupsPrintersBrowserProxy} */
  browserProxy_: null,

  /**
   * The number of printers we display if hidden printers are allowed.
   * kMinVisiblePrinters is the default value and we never show fewer printers
   * if the Show more button is visible.
   */
  visiblePrinterCounter_: kMinVisiblePrinters,

  /** @override */
  created() {
    this.browserProxy_ = settings.CupsPrintersBrowserProxyImpl.getInstance();
  },

  /**
   * Redoes the search whenever |searchTerm| or |savedPrinters| changes.
   * @private
   */
  onSearchOrPrintersChanged_() {
    if (!this.savedPrinters) {
      return;
    }

    const updatedPrinters = this.getVisiblePrinters_();

    this.updateList(
        'filteredPrinters_', printer => printer.printerInfo.printerId,
        updatedPrinters);
  },

  /**
   * @param {!CustomEvent<{target: !HTMLElement, item: !PrinterListEntry}>} e
   * @private
   */
  onOpenActionMenu_(e) {
    const item = /** @type {!PrinterListEntry} */ (e.detail.item);
    this.activePrinterListEntryIndex_ = this.savedPrinters.findIndex(
        printer => printer.printerInfo.printerId == item.printerInfo.printerId);
    this.activePrinter =
        this.get(['savedPrinters', this.activePrinterListEntryIndex_])
            .printerInfo;

    const target = /** @type {!HTMLElement} */ (e.detail.target);
    this.$$('cr-action-menu').showAt(target);
  },

  /** @private */
  onEditTap_() {
    // Event is caught by 'settings-cups-printers'.
    this.fire('edit-cups-printer-details');
    this.closeActionMenu_();
  },

  /** @private */
  onRemoveTap_() {
    this.browserProxy_.removeCupsPrinter(
        this.activePrinter.printerId, this.activePrinter.printerName);
    settings.recordSettingChange();
    this.activePrinter = null;
    this.activeListEntryIndex_ = -1;
    this.closeActionMenu_();
  },

  /** @private */
  onShowMoreTap_() {
    this.hasShowMoreBeenTapped_ = true;
  },

  /**
   * Gets the printers to be shown in the UI. These printers are filtered
   * by the search term, alphabetically sorted (if applicable), and are the
   * printers not hidden by the Show more section.
   * @return {!Array<!PrinterListEntry>} Returns only the visible printers.
   * @private
   */
  getVisiblePrinters_() {
    // Filter printers through |searchTerm|. If |searchTerm| is empty,
    // |filteredPrinters_| is just |savedPrinters|.
    const updatedPrinters = this.searchTerm ?
        this.savedPrinters.filter(
            item => settings.printing.matchesSearchTerm(
                item.printerInfo, this.searchTerm)) :
        this.savedPrinters.slice();

    updatedPrinters.sort(settings.printing.sortPrinters);

    this.moveNewlyAddedPrinters_(updatedPrinters, 0 /* toIndex */);

    if (this.shouldPrinterListBeCollapsed_()) {
      // If the Show more button is visible, we only display the first
      // N < |visiblePrinterCounter_| printers and the rest are hidden.
      return updatedPrinters.filter(
          (printer, idx) => idx < this.visiblePrinterCounter_);
    }
    return updatedPrinters;
  },

  /** @private */
  closeActionMenu_() {
    this.$$('cr-action-menu').close();
  },

  /**
   * @return {boolean} Returns true if the no search message should be visible.
   * @private
   */
  showNoSearchResultsMessage_() {
    return !!this.searchTerm && !this.filteredPrinters_.length;
  },

  /** @param{!Array<!PrinterListEntry>} addedPrinters */
  onSavedPrintersAdded(addedPrinters) {
    const currArr = this.newPrinters_.slice();
    for (const printer of addedPrinters) {
      this.visiblePrinterCounter_++;
      currArr.push(printer);
    }

    this.set('newPrinters_', currArr);
  },

  /** @param{!Array<!PrinterListEntry>} removedPrinters */
  onSavedPrintersRemoved(removedPrinters) {
    const currArr = this.newPrinters_.slice();
    for (const printer of removedPrinters) {
      const newPrinterRemovedIdx = currArr.findIndex(
          p => p.printerInfo.printerId == printer.printerInfo.printerId);
      // If the removed printer is a recently added printer, remove it from
      // |currArr|.
      if (newPrinterRemovedIdx > -1) {
        currArr.splice(newPrinterRemovedIdx, 1);
      }

      this.visiblePrinterCounter_ =
          Math.max(kMinVisiblePrinters, --this.visiblePrinterCounter_);
    }

    this.set('newPrinters_', currArr);
  },

  /**
   * Keeps track of whether the Show more button should be visible which means
   * that the printer list is collapsed. There are two ways a collapsed list
   * may be expanded: the Show more button is tapped or if there is a search
   * term.
   * @return {boolean} True if the printer list should be collapsed.
   * @private
   */
  shouldPrinterListBeCollapsed_() {
    // If |searchTerm| is set, never collapse the list.
    if (this.searchTerm) {
      return false;
    }

    // If |hasShowMoreBeenTapped_| is set to true, never collapse the list.
    if (this.hasShowMoreBeenTapped_) {
      return false;
    }

    // If the total number of saved printers does not exceed the number of
    // visible printers, there is no need for the list to be collapsed.
    if (this.savedPrinters.length - this.visiblePrinterCounter_ < 1) {
      return false;
    }

    return true;
  },

  /**
   * Moves printers that are in |newPrinters_| to position |toIndex| of
   * |printerArr|. This moves all recently added printers to the top of the
   * printer list.
   * @param {!Array<!PrinterListEntry>} printerArr
   * @param {number} toIndex
   * @private
   */
  moveNewlyAddedPrinters_(printerArr, toIndex) {
    if (!this.newPrinters_.length) {
      return;
    }

    // We have newly added printers, move them to the top of the list.
    for (const printer of this.newPrinters_) {
      const idx = printerArr.findIndex(
          p => p.printerInfo.printerId == printer.printerInfo.printerId);
      if (idx > -1) {
        moveEntryInPrinters(printerArr, idx, toIndex);
      }
    }
  },

  /**
   * @private
   * @return {number} Length of |filteredPrinters_|.
   */
  getFilteredPrintersLength_() {
    return this.filteredPrinters_.length;
  },
});
})();
