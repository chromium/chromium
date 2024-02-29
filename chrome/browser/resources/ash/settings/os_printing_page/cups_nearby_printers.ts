// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-nearby-printers' is a list container for
 * Nearby Printers.
 */
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../settings_shared.css.js';
import './cups_printer_types.js';
import './cups_printers_browser_proxy.js';
import './cups_printers_entry.js';

import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {getTemplate} from './cups_nearby_printers.html.js';
import {matchesSearchTerm, sortPrinters} from './cups_printer_dialog_util.js';
import {PrinterListEntry} from './cups_printer_types.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxyImpl, PrinterSetupResult} from './cups_printers_browser_proxy.js';
import {CupsPrintersEntryListMixin} from './cups_printers_entry_list_mixin.js';

const SettingsCupsNearbyPrintersElementBase =
    CupsPrintersEntryListMixin(WebUiListenerMixin(PolymerElement));

export class SettingsCupsNearbyPrintersElement extends
    SettingsCupsNearbyPrintersElementBase {
  static get is() {
    return 'settings-cups-nearby-printers';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Search term for filtering |nearbyPrinters|.
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

      activePrinter: {
        type: Object,
        notify: true,
      },

      printersCount: {
        type: Number,
        computed: 'getFilteredPrintersLength_(filteredPrinters_.*)',
        notify: true,
      },

      activePrinterListEntryIndex_: {
        type: Number,
        value: -1,
      },

      /**
       * List of printers filtered through a search term.
       */
      filteredPrinters_: {
        type: Array,
        value: () => [],
      },

      /**
       * Used by FocusRowBehavior to track the last focused element on a row.
       */
      lastFocused_: Object,

      /**
       * Used by FocusRowBehavior to track if the list has been blurred.
       */
      listBlurred_: Boolean,

      /**
       * This is set to true while waiting for a response during a printer
       * setup.
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

  activePrinter: CupsPrinterInfo;
  printersCount: number;
  searchTerm: string;
  userPrintersAllowed: boolean;

  private activePrinterListEntryIndex_: number;
  private filteredPrinters_: PrinterListEntry[];
  private lastFocused_: Object;
  private listBlurred_: boolean;
  private savingPrinter_: boolean;

  override ready(): void {
    super.ready();
    this.addEventListener(
        'add-automatic-printer',
        (event: CustomEvent<{item: PrinterListEntry}>) => {
          this.onAddAutomaticPrinter_(event);
        });

    this.addEventListener(
        'add-print-server-printer',
        (event: CustomEvent<{item: PrinterListEntry}>) => {
          this.onAddPrintServerPrinter_(event);
        });

    this.addEventListener(
        'query-discovered-printer',
        (event: CustomEvent<{item: PrinterListEntry}>) => {
          this.onQueryDiscoveredPrinter_(event);
        });
  }

  /**
   * Redoes the search whenever |searchTerm| or |nearbyPrinters| changes.
   */
  private onSearchOrPrintersChanged_(): void {
    if (!this.nearbyPrinters) {
      return;
    }
    // Filter printers through |searchTerm|. If |searchTerm| is empty,
    // |filteredPrinters_| is just |nearbyPrinters|.
    const updatedPrinters = this.searchTerm ?
        this.nearbyPrinters.filter(
            (item: PrinterListEntry) =>
                matchesSearchTerm(item.printerInfo, this.searchTerm)) :
        this.nearbyPrinters.slice();

    updatedPrinters.sort(sortPrinters);

    this.updateList(
        'filteredPrinters_',
        (printer: PrinterListEntry) => printer.printerInfo.printerId,
        updatedPrinters);
  }

  private onAddAutomaticPrinter_(e: CustomEvent<{item: PrinterListEntry}>):
      void {
    const item = e.detail.item;
    this.setActivePrinter_(item);
    this.savingPrinter_ = true;

    CupsPrintersBrowserProxyImpl.getInstance()
        .addDiscoveredPrinter(item.printerInfo.printerId)
        .then(
            this.onAddNearbyPrintersSucceeded_.bind(
                this, item.printerInfo.printerName),
            this.onAddNearbyPrinterFailed_.bind(this));
  }

  private onAddPrintServerPrinter_(e: CustomEvent<{item: PrinterListEntry}>):
      void {
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

  private onQueryDiscoveredPrinter_(e: CustomEvent<{item: PrinterListEntry}>):
      void {
    const item = e.detail.item;
    this.setActivePrinter_(item);
    this.savingPrinter_ = true;

    CupsPrintersBrowserProxyImpl.getInstance()
        .addDiscoveredPrinter(item.printerInfo.printerId)
        .then(
            this.onQueryDiscoveredPrinterSucceeded_.bind(
                this, item.printerInfo.printerName),
            this.onQueryDiscoveredPrinterFailed_.bind(this));
  }

  /**
   * Retrieves the index of |item| in |nearbyPrinters_| and sets that printer as
   * the active printer.
   */
  private setActivePrinter_(item: PrinterListEntry): void {
    this.activePrinterListEntryIndex_ = this.nearbyPrinters.findIndex(
        (printer: PrinterListEntry) =>
            printer.printerInfo.printerId === item.printerInfo.printerId);

    this.activePrinter =
        this.get(['nearbyPrinters', this.activePrinterListEntryIndex_])
            .printerInfo;
  }

  private showCupsPrinterToast_(
      resultCode: PrinterSetupResult, printerName: string): void {
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
   */
  private onAddNearbyPrintersSucceeded_(
      printerName: string, result: PrinterSetupResult): void {
    this.savingPrinter_ = false;
    this.showCupsPrinterToast_(result, printerName);
    recordSettingChange(Setting.kAddPrinter);
  }

  /**
   * Handler for addDiscoveredPrinter failure.
   */
  private onAddNearbyPrinterFailed_(printer: CupsPrinterInfo): void {
    this.savingPrinter_ = false;
    this.showCupsPrinterToast_(
        PrinterSetupResult.PRINTER_UNREACHABLE, printer.printerName);
  }

  /**
   * Handler for queryDiscoveredPrinter success.
   */
  private onQueryDiscoveredPrinterSucceeded_(
      printerName: string, result: PrinterSetupResult): void {
    this.savingPrinter_ = false;
    this.showCupsPrinterToast_(result, printerName);
    chrome.metricsPrivate.recordEnumerationValue(
        'Printing.CUPS.PrinterSetupResult.SettingsDiscoveredPrinters', result,
        Object.keys(PrinterSetupResult).length);
    recordSettingChange(Setting.kAddPrinter);
  }

  /**
   * Handler for queryDiscoveredPrinter failure.
   */
  private onQueryDiscoveredPrinterFailed_(printer: CupsPrinterInfo): void {
    this.savingPrinter_ = false;
    const openManufacturerDialogEvent = new CustomEvent(
        'open-manufacturer-model-dialog-for-specified-printer', {
          bubbles: true,
          composed: true,
          detail: {item: printer},
        });
    this.dispatchEvent(openManufacturerDialogEvent);
    chrome.metricsPrivate.recordEnumerationValue(
        'Printing.CUPS.PrinterSetupResult.SettingsDiscoveredPrinters',
        PrinterSetupResult.MANUAL_SETUP_REQUIRED,
        Object.keys(PrinterSetupResult).length);
  }

  /**
   * @return Returns true if the no search message should be visible.
   */
  private showNoSearchResultsMessage_(): boolean {
    return !!this.searchTerm && !this.filteredPrinters_.length;
  }

  private getFilteredPrintersLength_(): number {
    return this.filteredPrinters_.length;
  }

  /**
   * Forces the printer list to re-render all items.
   */
  resizePrintersList(): void {
    this.shadowRoot!.querySelector<IronListElement>(
                        '#printerEntryList')!.notifyResize();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cups-nearby-printers': SettingsCupsNearbyPrintersElement;
  }
  interface HTMLElementEventMap {
    'add-automatic-printer': CustomEvent<{item: PrinterListEntry}>;
    'add-print-server-printer': CustomEvent<{item: PrinterListEntry}>;
    'query-discovered-printer': CustomEvent<{item: PrinterListEntry}>;
  }
}

customElements.define(
    SettingsCupsNearbyPrintersElement.is, SettingsCupsNearbyPrintersElement);
