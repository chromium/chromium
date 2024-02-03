// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-add-printer-dialog' includes multiple dialogs to
 * set up a new CUPS printer.
 * Subdialogs include:
 * - 'add-printer-manually-dialog' is a dialog in which user can manually enter
 *   the information to set up a new printer.
 * - 'add-printer-manufacturer-model-dialog' is a dialog in which the user can
 *   manually select the manufacture and model of the new printer.
 * - 'add-print-server-dialog' is a dialog in which the user can
 *   add a print server.
 */

import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import './cups_add_print_server_dialog.js';
import './cups_add_printer_manually_dialog.js';
import './cups_add_printer_manufacturer_model_dialog.js';
import './cups_printer_shared.css.js';
import './cups_printers_browser_proxy.js';

import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CupsPrinterInfo} from './cups_printers_browser_proxy.js';
import {getTemplate} from './cups_settings_add_printer_dialog.html.js';

/**
 * Different dialogs in add printer flow.
 */
enum AddPrinterDialogs {
  MANUALLY = 'add-printer-manually-dialog',
  MANUFACTURER = 'add-printer-manufacturer-model-dialog',
  PRINTSERVER = 'add-print-server-dialog',
}

/**
 * Return a reset CupsPrinterInfo object.
 */
function getEmptyPrinter(): CupsPrinterInfo {
  return {
    isManaged: false,
    ppdManufacturer: '',
    ppdModel: '',
    printerAddress: '',
    printerDescription: '',
    printerId: '',
    printerMakeAndModel: '',
    printerName: '',
    printerPPDPath: '',
    printerPpdReference: {
      userSuppliedPpdUrl: '',
      effectiveMakeAndModel: '',
      autoconf: false,
    },
    printerProtocol: 'ipp',
    printerQueue: 'ipp/print',
    printServerUri: '',
  };
}

export class SettingsCupsAddPrinterDialogElement extends PolymerElement {
  static get is() {
    return 'settings-cups-add-printer-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      newPrinter: {
        type: Object,
      },

      previousDialog_: String,

      currentDialog_: String,

      showManuallyAddDialog_: {
        type: Boolean,
        value: false,
      },

      showManufacturerDialog_: {
        type: Boolean,
        value: false,
      },

      showAddPrintServerDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  newPrinter: CupsPrinterInfo;

  private currentDialog_: string;
  private previousDialog_: string;
  private showAddPrintServerDialog_: boolean;
  private showManuallyAddDialog_: boolean;
  private showManufacturerDialog_: boolean;

  override ready(): void {
    super.ready();

    this.addEventListener(
        'open-manually-add-printer-dialog', this.openManuallyAddPrinterDialog_);
    this.addEventListener(
        'open-manufacturer-model-dialog',
        this.openManufacturerModelDialogForCurrentPrinter_);
    this.addEventListener(
        'open-add-print-server-dialog', this.openPrintServerDialog_);
  }

  /**
   * Opens the Add manual printer dialog.
   */
  open(): void {
    this.resetData_();
    this.switchDialog_(
        '', AddPrinterDialogs.MANUALLY, 'showManuallyAddDialog_');
  }

  /**
   * Reset all the printer data in the Add printer flow.
   */
  private resetData_(): void {
    if (this.newPrinter) {
      this.newPrinter = getEmptyPrinter();
    }
  }

  private openManuallyAddPrinterDialog_(): void {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUALLY,
        'showManuallyAddDialog_');
  }

  private openManufacturerModelDialogForCurrentPrinter_(): void {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUFACTURER,
        'showManufacturerDialog_');
  }

  openManufacturerModelDialogForSpecifiedPrinter(printer: CupsPrinterInfo):
      void {
    this.newPrinter = printer;
    this.switchDialog_(
        '', AddPrinterDialogs.MANUFACTURER, 'showManufacturerDialog_');
  }

  private openPrintServerDialog_(): void {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.PRINTSERVER,
        'showAddPrintServerDialog_');
  }

  /**
   * Switch dialog from |fromDialog| to |toDialog|.
   * @param domIfBooleanName The name of the boolean variable
   *     corresponding to the |toDialog|.
   */
  private switchDialog_(
      fromDialog: string, toDialog: string, domIfBooleanName: string): void {
    this.previousDialog_ = fromDialog;
    this.currentDialog_ = toDialog;

    this.set(domIfBooleanName, true);

    microTask.run(() => {
      const dialog = this.shadowRoot!.querySelector(toDialog);
      dialog!.addEventListener('close', () => {
        this.set(domIfBooleanName, false);
      });
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cups-add-printer-dialog': SettingsCupsAddPrinterDialogElement;
  }
  interface HTMLElementEventMap {
    'open-manually-add-printer-dialog': CustomEvent;
    'open-manufacturer-model-dialog': CustomEvent;
    'open-add-print-server-dialog': CustomEvent;
    'close': CustomEvent;
  }
}

customElements.define(
    SettingsCupsAddPrinterDialogElement.is,
    SettingsCupsAddPrinterDialogElement);
