// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers-entry' is a component that holds a
 * printer.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../settings_shared_css.js';

import {FocusRowBehavior} from '//resources/js/cr/ui/focus_row_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import {PrinterListEntry, PrinterType} from './cups_printer_types.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, CupsPrintersList, ManufacturersInfo, ModelsInfo, PrinterMakeModel, PrinterPpdMakeModel, PrinterSetupResult, PrintServerResult} from './cups_printers_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-cups-printers-entry',

  behaviors: [
    FocusRowBehavior,
  ],
  properties: {
    /** @type {!PrinterListEntry} */
    printerEntry: Object,

    /**
     * TODO(jimmyxgong): Determine how subtext should be set and what
     * information it should have, including necessary ARIA labeling
     * The additional information subtext for a printer.
     * @type {string}
     */
    subtext: {type: String, value: ''},

    /**
     * This value is set to true if the printer is in saving mode.
     */
    savingPrinter: Boolean,

    /**
     * This value is set to true if UserPrintersAllowed policy is enabled.
     */
    userPrintersAllowed: {
      type: Boolean,
      value: false,
    }
  },

  /**
   * Fires a custom event when the menu button is clicked. Sends the details of
   * the printer and where the menu should appear.
   */
  onOpenActionMenuTap_(e) {
    this.fire('open-action-menu', {
      target: e.target,
      item: this.printerEntry,
    });
  },

  /** @private */
  onAddDiscoveredPrinterTap_(e) {
    this.fire('query-discovered-printer', {item: this.printerEntry});
  },

  /** @private */
  onAddAutomaticPrinterTap_() {
    this.fire('add-automatic-printer', {item: this.printerEntry});
  },

  /** @private */
  onAddServerPrinterTap_: function() {
    this.fire('add-print-server-printer', {item: this.printerEntry});
  },

  /**
   * @return {boolean}
   * @private
   */
  showActionsMenu_() {
    return this.printerEntry.printerType === PrinterType.SAVED ||
        this.printerEntry.printerType === PrinterType.ENTERPRISE;
  },

  /**
   * @return {boolean}
   * @private
   */
  isDiscoveredPrinter_() {
    return this.printerEntry.printerType === PrinterType.DISCOVERED;
  },

  /**
   * @return {boolean}
   * @private
   */
  isAutomaticPrinter_() {
    return this.printerEntry.printerType === PrinterType.AUTOMATIC;
  },

  /**
   * @return {boolean}
   * @private
   */
  isPrintServerPrinter_() {
    return this.printerEntry.printerType === PrinterType.PRINTSERVER;
  },

  /**
   * @return {boolean}
   * @private
   */
  isConfigureDisabled_() {
    return !this.userPrintersAllowed || this.savingPrinter;
  },

  getSaveButtonAria_() {
    return loadTimeData.getStringF(
        'savePrinterAria', this.printerEntry.printerInfo.printerName);
  },

  getSetupButtonAria_() {
    return loadTimeData.getStringF(
        'setupPrinterAria', this.printerEntry.printerInfo.printerName);
  },
});
