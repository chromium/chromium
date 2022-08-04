// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-nearby-printers' is a list container for
 * Nearby Printers.
 */
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './cups_printers_entry.js';
import '../../settings_shared.css.js';

import {ListPropertyUpdateBehavior, ListPropertyUpdateBehaviorInterface} from 'chrome://resources/js/list_property_update_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {matchesSearchTerm, sortPrinters} from './cups_printer_dialog_util.js';
import {PrinterListEntry} from './cups_printer_types.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, PrinterSetupResult} from './cups_printers_browser_proxy.js';
import {CupsPrintersEntryListBehavior, CupsPrintersEntryListBehaviorInterface} from './cups_printers_entry_list_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CupsPrintersEntryListBehaviorInterface}
 * @implements {ListPropertyUpdateBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsCupsNearbyPrintersElementBase = mixinBehaviors(
    [
      CupsPrintersEntryListBehavior,
      ListPropertyUpdateBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsCupsNearbyPrintersElement extends
    SettingsCupsNearbyPrintersElementBase {
  static get is() {
    return 'settings-cups-nearby-printers';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Search term for filtering |nearbyPrinters|.
       * @type {string}
       */
      searchTerm: {
        type: String,
        value: '',
      },

      /**
       * This value is set to true if UserPrintersAllowed policy is enabled.
       */
      userPrintersAllowed: {
        type: Boolean,
        value: false,
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
       * Used by FocusRowBehavior to track the last focused element on a row.
       * @private
       */
      lastFocused_: Object,

      /**
       * Used by FocusRowBehavior to track if the list has been blurred.
       * @private
       */
      listBlurred_: Boolean,

      /**
       * This is set to true while waiting for a response during a printer
       * setup.
       * @type {boolean}
       * @private
       */
      savingPrinter_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return ['onSearchOrPrintersChanged_(nearbyPrinters.*, searchTerm)'];
  }

  ready() {
    super.ready();
    this.addEventListener('add-automatic-printer', (event) => {
      this.onAddAutomaticPrinter_(
          /** @type {!CustomEvent<{item: !PrinterListEntry}>} */ (event));
    });

    this.addEventListener('add-print-server-printer', (event) => {
      this.onAddPrintServerPrinter_(
          /** @type {!CustomEvent<{item: !PrinterListEntry}>} */ (event));
    });

    this.addEventListener('query-discovered-printer', (event) => {
      this.onQueryDiscoveredPrinter_(
          /** @type {!CustomEvent<{item: !PrinterListEntry}>} */ (event));
    });
  }

  /**
   * Redoes the search whenever |searchTerm| or |nearbyPrinters| changes.
   * @private
   */
  onSearchOrPrintersChanged_() {
    if (!this.nearbyPrinters) {
      return;
    }
    // Filter printers through |searchTerm|. If |searchTerm| is empty,
    // |filteredPrinters_| is just |nearbyPrinters|.
    const updatedPrinters = this.searchTerm ?
        this.nearbyPrinters.filter(
            item => matchesSearchTerm(item.printerInfo, this.searchTerm)) :
        this.nearbyPrinters.slice();

    updatedPrinters.sort(sortPrinters);

    this.updateList(
        'filteredPrinters_', printer => printer.printerInfo.printerId,
        updatedPrinters);
  }

  /**
   * @param {!CustomEvent<{item: !PrinterListEntry}>} e
   * @private
   */
  onAddAutomaticPrinter_(e) {
    const item = e.detail.item;
    this.setActivePrinter_(item);
    this.savingPrinter_ = true;

    CupsPrintersBrowserProxyImpl.getInstance()
        .addDiscoveredPrinter(item.printerInfo.printerId)
        .then(
            this.onAddNearbyPrintersSucceeded_.bind(
                this, item.printerInfo.printerName),
            this.onAddNearbyPrinterFailed_.bind(this));
    recordSettingChange();
  }

  /**
   * @param {!CustomEvent<{item: !PrinterListEntry}>} e
   * @private
   */
  onAddPrintServerPrinter_(e) {
    const item = e.detail.item;
    this.setActivePrinter_(item);
    this.savingPrinter_ = true;

    CupsPrintersBrowserProxyImpl.getInstance()
        .addCupsPrinter(item.printerInfo)
        .then(
            this.onAddNearbyPrintersSucceeded_.bind(
                this, item.printerInfo.printerName),
            this.onAddNearbyPrinterFailed_.bind(this));
  }

  /**
   * @param {!CustomEvent<{item: !PrinterListEntry}>} e
   * @private
   */
  onQueryDiscoveredPrinter_(e) {
    const item = e.detail.item;
    this.setActivePrinter_(item);
    this.savingPrinter_ = true;

    // This is a workaround to ensure type safety on the params of the casted
    // function. We do this because the closure compiler does not work well with
    // rejected js promises.
    const queryDiscoveredPrinterFailed = /** @type {!Function}) */ (
        this.onQueryDiscoveredPrinterFailed_.bind(this));
    CupsPrintersBrowserProxyImpl.getInstance()
        .addDiscoveredPrinter(item.printerInfo.printerId)
        .then(
            this.onQueryDiscoveredPrinterSucceeded_.bind(
                this, item.printerInfo.printerName),
            queryDiscoveredPrinterFailed);
    recordSettingChange();
  }

  /**
   * Retrieves the index of |item| in |nearbyPrinters_| and sets that printer as
   * the active printer.
   * @param {!PrinterListEntry} item
   * @private
   */
  setActivePrinter_(item) {
    this.activePrinterListEntryIndex_ = this.nearbyPrinters.findIndex(
        printer =>
            printer.printerInfo.printerId === item.printerInfo.printerId);

    this.activePrinter =
        this.get(['nearbyPrinters', this.activePrinterListEntryIndex_])
            .printerInfo;
  }

  /**
   * @param {!PrinterSetupResult} resultCode
   * @param {string} printerName
   * @private
   */
  showCupsPrinterToast_(resultCode, printerName) {
    const event = new CustomEvent('show-cups-printer-toast', {
      bubbles: true,
      composed: true,
      detail: {
        resultCode,
        printerName,
      },
    });
    this.dispatchEvent(event);
  }

  /**
   * Handler for addDiscoveredPrinter success.
   * @param {string} printerName
   * @param {!PrinterSetupResult} result
   * @private
   */
  onAddNearbyPrintersSucceeded_(printerName, result) {
    this.savingPrinter_ = false;
    this.showCupsPrinterToast_(result, printerName);
  }

  /**
   * Handler for addDiscoveredPrinter failure.
   * @param {*} printer
   * @private
   */
  onAddNearbyPrinterFailed_(printer) {
    this.savingPrinter_ = false;
    this.showCupsPrinterToast_(
        PrinterSetupResult.PRINTER_UNREACHABLE, printer.printerName);
  }

  /**
   * Handler for queryDiscoveredPrinter success.
   * @param {string} printerName
   * @param {!PrinterSetupResult} result
   * @private
   */
  onQueryDiscoveredPrinterSucceeded_(printerName, result) {
    this.savingPrinter_ = false;
    this.showCupsPrinterToast_(result, printerName);
  }

  /**
   * Handler for queryDiscoveredPrinter failure.
   * @param {!CupsPrinterInfo} printer
   * @private
   */
  onQueryDiscoveredPrinterFailed_(printer) {
    this.savingPrinter_ = false;
    const openManufacturerDialogEvent = new CustomEvent(
        'open-manufacturer-model-dialog-for-specified-printer', {
          bubbles: true,
          composed: true,
          detail: {item: /** @type {CupsPrinterInfo} */ (printer)},
        });
    this.dispatchEvent(openManufacturerDialogEvent);
  }

  /**
   * @return {boolean} Returns true if the no search message should be visible.
   * @private
   */
  showNoSearchResultsMessage_() {
    return !!this.searchTerm && !this.filteredPrinters_.length;
  }

  /**
   * @private
   * @return {number} Length of |filteredPrinters_|.
   */
  getFilteredPrintersLength_() {
    return this.filteredPrinters_.length;
  }
}

customElements.define(
    SettingsCupsNearbyPrintersElement.is, SettingsCupsNearbyPrintersElement);
