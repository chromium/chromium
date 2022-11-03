// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers-entry' is a component that holds a
 * printer.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../settings_shared.css.js';
import './cups_printer_types.js';

import {FocusRowMixin} from 'chrome://resources/js/focus_row_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import {PrinterListEntry, PrinterType} from './cups_printer_types.js';
import {getTemplate} from './cups_printers_entry.html.js';

const SettingsCupsPrintersEntryElementBase = FocusRowMixin(PolymerElement);

export class SettingsCupsPrintersEntryElement extends
    SettingsCupsPrintersEntryElementBase {
  static get is() {
    return 'settings-cups-printers-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      printerEntry: Object,


      /**
       * TODO(jimmyxgong): Determine how subtext should be set and what
       * information it should have, including necessary ARIA labeling
       * The additional information subtext for a printer.
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

  printerEntry: PrinterListEntry;
  savingPrinter: boolean;
  subtext: string;
  userPrintersAllowed: boolean;

  /**
   * Fires a custom event when the menu button is clicked. Sends the details of
   * the printer and where the menu should appear.
   */
  private onOpenActionMenuTap_(
      e: CustomEvent<{target: HTMLElement, item: PrinterListEntry}>): void {
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

  private onAddDiscoveredPrinterTap_(): void {
    const queryDiscoveredPrinterEvent =
        new CustomEvent('query-discovered-printer', {
          bubbles: true,
          composed: true,
          detail: {item: this.printerEntry},
        });
    this.dispatchEvent(queryDiscoveredPrinterEvent);
  }

  private onAddAutomaticPrinterTap_(): void {
    const addAutomaticPrinterEvent = new CustomEvent('add-automatic-printer', {
      bubbles: true,
      composed: true,
      detail: {item: this.printerEntry},
    });
    this.dispatchEvent(addAutomaticPrinterEvent);
  }

  private onAddServerPrinterTap_(): void {
    const addPrintServer = new CustomEvent('add-print-server-printer', {
      bubbles: true,
      composed: true,
      detail: {item: this.printerEntry},
    });
    this.dispatchEvent(addPrintServer);
  }

  private showActionsMenu_(): boolean {
    return this.printerEntry.printerType === PrinterType.SAVED ||
        this.printerEntry.printerType === PrinterType.ENTERPRISE;
  }

  private isDiscoveredPrinter_(): boolean {
    return this.printerEntry.printerType === PrinterType.DISCOVERED;
  }

  private isAutomaticPrinter_(): boolean {
    return this.printerEntry.printerType === PrinterType.AUTOMATIC;
  }

  private isPrintServerPrinter_(): boolean {
    return this.printerEntry.printerType === PrinterType.PRINTSERVER;
  }

  private isConfigureDisabled_(): boolean {
    return !this.userPrintersAllowed || this.savingPrinter;
  }

  private getSaveButtonAria_(): string {
    return loadTimeData.getStringF(
        'savePrinterAria', this.printerEntry.printerInfo.printerName);
  }

  private getSetupButtonAria_(): string {
    return loadTimeData.getStringF(
        'setupPrinterAria', this.printerEntry.printerInfo.printerName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cups-printers-entry': SettingsCupsPrintersEntryElement;
  }
}

customElements.define(
    SettingsCupsPrintersEntryElement.is, SettingsCupsPrintersEntryElement);
