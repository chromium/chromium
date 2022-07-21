// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-enterprise-printers' is a list container for
 * Enterprise Printers.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './cups_printers_entry.js';
import '../../settings_shared.css.js';

import {ListPropertyUpdateBehavior, ListPropertyUpdateBehaviorInterface} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {matchesSearchTerm, sortPrinters} from './cups_printer_dialog_util.js';
import {PrinterListEntry} from './cups_printer_types.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl} from './cups_printers_browser_proxy.js';
import {CupsPrintersEntryListBehavior, CupsPrintersEntryListBehaviorInterface} from './cups_printers_entry_list_behavior.js';

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
 * @constructor
 * @extends {PolymerElement}
 * @implements {CupsPrintersEntryListBehaviorInterface}
 * @implements {ListPropertyUpdateBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsCupsEnterprisePrintersElementBase = mixinBehaviors(
    [
      CupsPrintersEntryListBehavior,
      ListPropertyUpdateBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsCupsEnterprisePrintersElement extends
    SettingsCupsEnterprisePrintersElementBase {
  static get is() {
    return 'settings-cups-enterprise-printers';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Search term for filtering |enterprisePrinters|.
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

      /**
       * @type {number}
       * @private
       */
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
       * @type {!Array<!PrinterListEntry>}
       * @private
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
       * @private
       */
      hasShowMoreBeenTapped_: {
        type: Boolean,
        value: false,
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
    };
  }

  static get observers() {
    return [
      'onSearchOrPrintersChanged_(enterprisePrinters.*, searchTerm, ' +
          'hasShowMoreBeenTapped_)',
    ];
  }

  /** @override */
  constructor() {
    super();

    /** @private {!CupsPrintersBrowserProxy} */
    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();

    /**
     * The number of printers we display if hidden printers are allowed.
     * kMinVisiblePrinters is the default value and we never show fewer printers
     * if the Show more button is visible.
     */
    this.visiblePrinterCounter_ = kMinVisiblePrinters;
  }

  ready() {
    super.ready();
    this.addEventListener('open-action-menu', (event) => {
      this.onOpenActionMenu_(
          /**
             @type {!CustomEvent<{target: !HTMLElement, item:
                 !PrinterListEntry}>}
           */
          (event));
    });
  }

  /**
   * Redoes the search whenever |searchTerm| or |enterprisePrinters| changes.
   * @private
   */
  onSearchOrPrintersChanged_() {
    if (!this.enterprisePrinters) {
      return;
    }
    // Filter printers through |searchTerm|. If |searchTerm| is empty,
    // |filteredPrinters_| is just |enterprisePrinters|.
    let updatedPrinters = this.searchTerm ?
        this.enterprisePrinters.filter(
            item => matchesSearchTerm(item.printerInfo, this.searchTerm)) :
        this.enterprisePrinters.slice();
    updatedPrinters.sort(sortPrinters);

    if (this.shouldPrinterListBeCollapsed_()) {
      // If the Show more button is visible, we only display the first
      // N < |visiblePrinterCounter_| printers and the rest are hidden.
      updatedPrinters = updatedPrinters.filter(
          (printer, idx) => idx < this.visiblePrinterCounter_);
    }

    this.updateList(
        'filteredPrinters_', printer => printer.printerInfo.printerId,
        updatedPrinters);
  }

  /** @private */
  onShowMoreTap_() {
    this.hasShowMoreBeenTapped_ = true;
  }

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

    // If the total number of enterprise printers does not exceed the number of
    // visible printers, there is no need for the list to be collapsed.
    if (this.enterprisePrinters.length - this.visiblePrinterCounter_ < 1) {
      return false;
    }

    return true;
  }

  /**
   * @return {boolean} Returns true if the no search message should be visible.
   * @private
   */
  showNoSearchResultsMessage_() {
    return !!this.searchTerm && !this.filteredPrinters_.length;
  }

  /**
   * @return {number} Length of |filteredPrinters_|.
   * @private
   */
  getFilteredPrintersLength_() {
    return this.filteredPrinters_.length;
  }

  /**
   * @param {!CustomEvent<{target: !HTMLElement, item: !PrinterListEntry}>} e
   * @private
   */
  onOpenActionMenu_(e) {
    const item = /** @type {!PrinterListEntry} */ (e.detail.item);
    this.activePrinterListEntryIndex_ = this.enterprisePrinters.findIndex(
        printer =>
            printer.printerInfo.printerId === item.printerInfo.printerId);
    this.activePrinter =
        this.get(['enterprisePrinters', this.activePrinterListEntryIndex_])
            .printerInfo;

    const target = /** @type {!HTMLElement} */ (e.detail.target);
    this.shadowRoot.querySelector('cr-action-menu').showAt(target);
  }

  /** @private */
  onViewTap_() {
    // Event is caught by 'settings-cups-printers'.
    const editCupsPrinterDetailsEvent = new CustomEvent(
        'edit-cups-printer-details', {bubbles: true, composed: true});
    this.dispatchEvent(editCupsPrinterDetailsEvent);
    this.closeActionMenu_();
  }

  /** @private */
  closeActionMenu_() {
    this.shadowRoot.querySelector('cr-action-menu').close();
  }
}

customElements.define(
    SettingsCupsEnterprisePrintersElement.is,
    SettingsCupsEnterprisePrintersElement);
