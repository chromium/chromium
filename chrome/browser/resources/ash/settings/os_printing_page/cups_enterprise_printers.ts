// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-enterprise-printers' is a list container for
 * Enterprise Printers.
 */

import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../settings_shared.css.js';
import './cups_printer_types.js';
import './cups_printers_browser_proxy.js';
import './cups_printers_entry.js';

import {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './cups_enterprise_printers.html.js';
import {matchesSearchTerm, sortPrinters} from './cups_printer_dialog_util.js';
import {PrinterListEntry} from './cups_printer_types.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl} from './cups_printers_browser_proxy.js';
import {CupsPrintersEntryListMixin} from './cups_printers_entry_list_mixin.js';

/**
 * If the Show more button is visible, the minimum number of printers we show
 * is 3.
 */
const MIN_VISIBLE_PRINTERS = 3;

/**
 * Move a printer's position in |printerArr| from |fromIndex| to |toIndex|.
 */
export function moveEntryInPrinters(
    printerArr: PrinterListEntry[], fromIndex: number, toIndex: number): void {
  const element = printerArr[fromIndex];
  printerArr.splice(fromIndex, 1);
  printerArr.splice(toIndex, 0, element);
}

const SettingsCupsEnterprisePrintersElementBase =
    CupsPrintersEntryListMixin(WebUiListenerMixin(PolymerElement));

export class SettingsCupsEnterprisePrintersElement extends
    SettingsCupsEnterprisePrintersElementBase {
  static get is() {
    return 'settings-cups-enterprise-printers';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {

      /**
       * Search term for filtering |enterprisePrinters|.
       */
      searchTerm: {
        type: String,
        value: '',
      },

      activePrinter: {
        type: Object,
        notify: true,
      },

      activePrinterListEntryIndex_: {
        type: Number,
        value: -1,
      },

      printersCount: {
        type: Number,
        computed: 'getFilteredPrintersLength_(filteredPrinters_.*)',
        notify: true,
      },

      /**
       * List of printers filtered through a search term.
       */
      filteredPrinters_: {
        type: Array,
        value: () => [],
      },

      /**
       * Keeps track of whether the user has tapped the Show more button. A
       * search term will expand the collapsed list, so we need to keep track of
       * whether the list expanded because of a search term or because the user
       * tapped on the Show more button.
       */
      hasShowMoreBeenTapped_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by FocusRowBehavior to track the last focused element on a row.
       */
      lastFocused_: Object,

      /**
       * Used by FocusRowBehavior to track if the list has been blurred.
       */
      listBlurred_: Boolean,
    };
  }

  static get observers() {
    return [
      'onSearchOrPrintersChanged_(enterprisePrinters.*, searchTerm, ' +
          'hasShowMoreBeenTapped_)',
    ];
  }

  activePrinter: CupsPrinterInfo;
  printersCount: number;
  searchTerm: string;

  private activePrinterListEntryIndex_: number;
  private browserProxy_: CupsPrintersBrowserProxy;
  private filteredPrinters_: PrinterListEntry[];
  private hasShowMoreBeenTapped_: boolean;
  private lastFocused_: Object;
  private listBlurred_: boolean;
  private visiblePrinterCounter_: number;

  constructor() {
    super();

    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();


    /**
     * The number of printers we display if hidden printers are allowed.
     * MIN_VISIBLE_PRINTERS is the default value and we never show fewer
     * printers if the Show more button is visible.
     */
    this.visiblePrinterCounter_ = MIN_VISIBLE_PRINTERS;
  }

  override ready(): void {
    super.ready();
    this.addEventListener('open-action-menu', event => {
      this.onOpenActionMenu_(event);
    });
  }

  /**
   * Redoes the search whenever |searchTerm| or |enterprisePrinters| changes.
   */
  private onSearchOrPrintersChanged_(): void {
    if (!this.enterprisePrinters) {
      return;
    }
    /**
     * Filter printers through |searchTerm|. If |searchTerm| is empty,
     * |filteredPrinters_| is just |enterprisePrinters|.
     */
    let updatedPrinters = this.searchTerm ?
        this.enterprisePrinters.filter(
            (item: PrinterListEntry) =>
                matchesSearchTerm(item.printerInfo, this.searchTerm)) :
        this.enterprisePrinters.slice();
    updatedPrinters.sort(sortPrinters);

    if (this.shouldPrinterListBeCollapsed_()) {
      // If the Show more button is visible, we only display the first
      // N < |visiblePrinterCounter_| printers and the rest are hidden.
      updatedPrinters = updatedPrinters.filter(
          (_: PrinterListEntry, idx: number) =>
              idx < this.visiblePrinterCounter_);
    }

    this.updateList(
        'filteredPrinters_',
        (printer: PrinterListEntry) => printer.printerInfo.printerId,
        updatedPrinters);
  }

  private onShowMoreClick_(): void {
    this.hasShowMoreBeenTapped_ = true;
  }


  /**
   * Keeps track of whether the Show more button should be visible which means
   * that the printer list is collapsed. There are two ways a collapsed list
   * may be expanded: the Show more button is tapped or if there is a search
   * term.
   */
  private shouldPrinterListBeCollapsed_(): boolean {
    // If |searchTerm| is set, never collapse the list.
    if (this.searchTerm) {
      return false;
    }

    // If |hasShowMoreBeenTapped_| is set to true, never collapse the list.
    if (this.hasShowMoreBeenTapped_) {
      return false;
    }

    // If the total number of enterprise printers does not exceed the number of
    // visible printers, there is no need for the list to be collapsed.
    if (this.enterprisePrinters.length - this.visiblePrinterCounter_ < 1) {
      return false;
    }

    return true;
  }

  private showNoSearchResultsMessage_(): boolean {
    return !!this.searchTerm && !this.filteredPrinters_.length;
  }

  private getFilteredPrintersLength_(): number {
    return this.filteredPrinters_.length;
  }

  private getCrActionMenu(): CrActionMenuElement {
    return castExists(this.shadowRoot!.querySelector('cr-action-menu'));
  }

  private onOpenActionMenu_(
      e: CustomEvent<{target: HTMLElement, item: PrinterListEntry}>): void {
    const item: PrinterListEntry = e.detail.item;
    this.activePrinterListEntryIndex_ = this.enterprisePrinters.findIndex(
        (printer: PrinterListEntry) =>
            printer.printerInfo.printerId === item.printerInfo.printerId);
    this.activePrinter =
        this.get(['enterprisePrinters', this.activePrinterListEntryIndex_])
            .printerInfo;

    const target: HTMLElement = e.detail.target;
    this.getCrActionMenu().showAt(target);
  }

  private onViewClick_(): void {
    // Event is caught by 'settings-cups-printers'.
    const editCupsPrinterDetailsEvent = new CustomEvent(
        'edit-cups-printer-details', {bubbles: true, composed: true});
    this.dispatchEvent(editCupsPrinterDetailsEvent);
    this.closeActionMenu_();
  }

  private closeActionMenu_(): void {
    this.getCrActionMenu().close();
  }
}

declare global {
  interface HTMLElementEventMap {
    'open-action-menu':
        CustomEvent<{target: HTMLElement, item: PrinterListEntry}>;
  }
  interface HTMLElementTagNameMap {
    'settings-cups-enterprise-printers': SettingsCupsEnterprisePrintersElement;
  }
}

customElements.define(
    SettingsCupsEnterprisePrintersElement.is,
    SettingsCupsEnterprisePrintersElement);
