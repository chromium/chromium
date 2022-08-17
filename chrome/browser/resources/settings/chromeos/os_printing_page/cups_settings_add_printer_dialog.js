// Copyright 2020 The Chromium Authors. All rights reserved.
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

import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './cups_add_print_server_dialog.js';
import './cups_add_printer_manually_dialog.js';
import './cups_add_printer_manufacturer_model_dialog.js';
import './cups_printer_shared_css.js';

import {html, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CupsPrinterInfo} from './cups_printers_browser_proxy.js';

/**
 * Different dialogs in add printer flow.
 * @enum {string}
 */
const AddPrinterDialogs = {
  MANUALLY: 'add-printer-manually-dialog',
  MANUFACTURER: 'add-printer-manufacturer-model-dialog',
  PRINTSERVER: 'add-print-server-dialog',
};

/**
 * Return a reset CupsPrinterInfo object.
 *  @return {!CupsPrinterInfo}
 */
function getEmptyPrinter_() {
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
    printerStatus: '',
    printServerUri: '',
  };
}

/** @polymer */
class SettingsCupsAddPrinterDialogElement extends PolymerElement {
  static get is() {
    return 'settings-cups-add-printer-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!CupsPrinterInfo} */
      newPrinter: {
        type: Object,
      },

      /** @private {string} */
      previousDialog_: String,

      /** @private {string} */
      currentDialog_: String,

      /** @private {boolean} */
      showManuallyAddDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      showManufacturerDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      showAddPrintServerDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  ready() {
    super.ready();

    this.addEventListener(
        'open-manually-add-printer-dialog', this.openManuallyAddPrinterDialog_);
    this.addEventListener(
        'open-manufacturer-model-dialog',
        this.openManufacturerModelDialogForCurrentPrinter_);
    this.addEventListener(
        'open-add-print-server-dialog', this.openPrintServerDialog_);
  }

  /** Opens the Add manual printer dialog. */
  open() {
    this.resetData_();
    this.switchDialog_(
        '', AddPrinterDialogs.MANUALLY, 'showManuallyAddDialog_');
  }

  /**
   * Reset all the printer data in the Add printer flow.
   * @private
   */
  resetData_() {
    if (this.newPrinter) {
      this.newPrinter = getEmptyPrinter_();
    }
  }

  /** @private */
  openManuallyAddPrinterDialog_() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUALLY,
        'showManuallyAddDialog_');
  }

  /** @private */
  openManufacturerModelDialogForCurrentPrinter_() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUFACTURER,
        'showManufacturerDialog_');
  }

  /** @param {!CupsPrinterInfo} printer */
  openManufacturerModelDialogForSpecifiedPrinter(printer) {
    this.newPrinter = printer;
    this.switchDialog_(
        '', AddPrinterDialogs.MANUFACTURER, 'showManufacturerDialog_');
  }

  /** @private */
  openPrintServerDialog_() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.PRINTSERVER,
        'showAddPrintServerDialog_');
  }

  /**
   * Switch dialog from |fromDialog| to |toDialog|.
   * @param {string} fromDialog
   * @param {string} toDialog
   * @param {string} domIfBooleanName The name of the boolean variable
   *     corresponding to the |toDialog|.
   * @private
   */
  switchDialog_(fromDialog, toDialog, domIfBooleanName) {
    this.previousDialog_ = fromDialog;
    this.currentDialog_ = toDialog;

    this.set(domIfBooleanName, true);
    microTask.run(() => {
      const dialog = this.shadowRoot.querySelector(toDialog);
      dialog.addEventListener('close', () => {
        this.set(domIfBooleanName, false);
      });
    });
  }
}

customElements.define(
    SettingsCupsAddPrinterDialogElement.is,
    SettingsCupsAddPrinterDialogElement);
