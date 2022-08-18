// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers-entry' is a component that holds a
 * printer.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../settings_shared.css.js';

import {FocusRowBehavior, FocusRowBehaviorInterface} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import {PrinterListEntry, PrinterType} from './cups_printer_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {FocusRowBehaviorInterface}
 */
const SettingsCupsPrintersEntryElementBase =
    mixinBehaviors([FocusRowBehavior], PolymerElement);

/** @polymer */
class SettingsCupsPrintersEntryElement extends
    SettingsCupsPrintersEntryElementBase {
  static get is() {
    return 'settings-cups-printers-entry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
      },
    };
  }

  /**
   * Fires a custom event when the menu button is clicked. Sends the details of
   * the printer and where the menu should appear.
   */
  onOpenActionMenuTap_(e) {
    const openActionMenuEvent = new CustomEvent('open-action-menu', {
      bubbles: true,
      composed: true,
      detail: {
        target: e.target,
        item: this.printerEntry,
      },
    });
    this.dispatchEvent(openActionMenuEvent);
  }

  /** @private */
  onAddDiscoveredPrinterTap_(e) {
    const queryDiscoveredPrinterEvent =
        new CustomEvent('query-discovered-printer', {
          bubbles: true,
          composed: true,
          detail: {item: this.printerEntry},
        });
    this.dispatchEvent(queryDiscoveredPrinterEvent);
  }

  /** @private */
  onAddAutomaticPrinterTap_() {
    const addAutomaticPrinterEvent = new CustomEvent('add-automatic-printer', {
      bubbles: true,
      composed: true,
      detail: {item: this.printerEntry},
    });
    this.dispatchEvent(addAutomaticPrinterEvent);
  }

  /** @private */
  onAddServerPrinterTap_() {
    const addPrintServer = new CustomEvent('add-print-server-printer', {
      bubbles: true,
      composed: true,
      detail: {item: this.printerEntry},
    });
    this.dispatchEvent(addPrintServer);
  }

  /**
   * @return {boolean}
   * @private
   */
  showActionsMenu_() {
    return this.printerEntry.printerType === PrinterType.SAVED ||
        this.printerEntry.printerType === PrinterType.ENTERPRISE;
  }

  /**
   * @return {boolean}
   * @private
   */
  isDiscoveredPrinter_() {
    return this.printerEntry.printerType === PrinterType.DISCOVERED;
  }

  /**
   * @return {boolean}
   * @private
   */
  isAutomaticPrinter_() {
    return this.printerEntry.printerType === PrinterType.AUTOMATIC;
  }

  /**
   * @return {boolean}
   * @private
   */
  isPrintServerPrinter_() {
    return this.printerEntry.printerType === PrinterType.PRINTSERVER;
  }

  /**
   * @return {boolean}
   * @private
   */
  isConfigureDisabled_() {
    return !this.userPrintersAllowed || this.savingPrinter;
  }

  getSaveButtonAria_() {
    return loadTimeData.getStringF(
        'savePrinterAria', this.printerEntry.printerInfo.printerName);
  }

  getSetupButtonAria_() {
    return loadTimeData.getStringF(
        'setupPrinterAria', this.printerEntry.printerInfo.printerName);
  }
}

customElements.define(
    SettingsCupsPrintersEntryElement.is, SettingsCupsPrintersEntryElement);
