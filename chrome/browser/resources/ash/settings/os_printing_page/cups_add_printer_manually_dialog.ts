// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-printer-manually-dialog' is a dialog in which user can
 * manually enter the information to set up a new printer.
 */

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import './cups_add_print_server_dialog.js';
import './cups_add_printer_dialog.js';
import './cups_printer_dialog_error.js';
import './cups_printer_shared.css.js';
import './cups_printers_browser_proxy.js';

import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast, castExists} from '../assert_extras.js';

import {AddPrinterDialogElement} from './cups_add_printer_dialog.js';
import {getTemplate} from './cups_add_printer_manually_dialog.html.js';
import {getErrorText, isNameAndAddressValid} from './cups_printer_dialog_util.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, PrinterMakeModel, PrinterSetupResult} from './cups_printers_browser_proxy.js';

function getEmptyPrinter(): object {
  return {
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

export interface AddPrinterManuallyDialogElement {
  $: {printerAddressInput: CrInputElement};
}

export class AddPrinterManuallyDialogElement extends PolymerElement {
  static get is() {
    return 'add-printer-manually-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      newPrinter: {type: Object, notify: true, value: getEmptyPrinter},

      addPrinterInProgress_: {
        type: Boolean,
        value: false,
      },

      /**
       * The error text to be displayed on the dialog.
       */
      errorText_: {
        type: String,
        value: '',
      },

      showPrinterQueue_: {
        type: Boolean,
        value: true,
      },
    };
  }

  static get observers() {
    return ['printerInfoChanged_(newPrinter.*)'];
  }

  newPrinter: CupsPrinterInfo;

  private addPrinterInProgress_: boolean;
  private browserProxy_: CupsPrintersBrowserProxy;
  private errorText_: string;
  private showPrinterQueue_: boolean;

  constructor() {
    super();

    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();
  }

  private getAddPrinterDialog_(): AddPrinterDialogElement {
    return castExists(this.shadowRoot!.querySelector('add-printer-dialog'));
  }
  private onCancelClick_(): void {
    this.getAddPrinterDialog_().close();
  }

  private onAddPrinterSucceeded_(result: PrinterSetupResult): void {
    this.recordAddPrinterResult(/*success=*/ true);
    const showCupsPrinterToastEvent =
        new CustomEvent('show-cups-printer-toast', {
          bubbles: true,
          composed: true,
          detail: {
            resultCode: result,
            printerName: this.newPrinter.printerName,
          },
        });
    this.dispatchEvent(showCupsPrinterToastEvent);
    this.getAddPrinterDialog_().close();
  }

  private onAddPrinterFailed_(result: PrinterSetupResult): void {
    this.recordAddPrinterResult(/*success=*/ false);
    this.errorText_ = getErrorText(result);
  }

  private openManufacturerModelDialog_(): void {
    const event = new CustomEvent('open-manufacturer-model-dialog', {
      bubbles: true,
      composed: true,
    });
    this.dispatchEvent(event);
  }

  private onPrinterFound_(info: PrinterMakeModel): void {
    const newPrinter: CupsPrinterInfo = Object.assign({}, this.newPrinter);

    newPrinter.printerMakeAndModel = info.makeAndModel;
    newPrinter.printerPpdReference.userSuppliedPpdUrl =
        info.ppdRefUserSuppliedPpdUrl;
    newPrinter.printerPpdReference.effectiveMakeAndModel =
        info.ppdRefEffectiveMakeAndModel;
    newPrinter.printerPpdReference.autoconf = info.autoconf;

    this.newPrinter = newPrinter;

    // Add the printer if it's configurable. Otherwise, forward to the
    // manufacturer dialog.
    if (info.ppdReferenceResolved) {
      this.browserProxy_.addCupsPrinter(this.newPrinter)
          .then(
              this.onAddPrinterSucceeded_.bind(this),
              this.onAddPrinterFailed_.bind(this));
    } else {
      this.getAddPrinterDialog_()!.close();
      this.openManufacturerModelDialog_();
    }
  }

  private infoFailed_(result: PrinterSetupResult): void {
    this.recordAddPrinterResult(/*success=*/ false);
    this.addPrinterInProgress_ = false;
    if (result === PrinterSetupResult.PRINTER_UNREACHABLE) {
      this.$.printerAddressInput.invalid = true;
      return;
    }
    this.errorText_ = getErrorText(result);
  }

  private addPressed_(): void {
    this.addPrinterInProgress_ = true;

    if (this.newPrinter.printerProtocol === 'ipp' ||
        this.newPrinter.printerProtocol === 'ipps') {
      this.browserProxy_.getPrinterInfo(this.newPrinter)
          .then(this.onPrinterFound_.bind(this), this.infoFailed_.bind(this));
    } else {
      this.getAddPrinterDialog_()!.close();
      this.openManufacturerModelDialog_();
    }
  }

  private onPrintServerClick_(): void {
    this.getAddPrinterDialog_()!.close();

    const openAddPrintServerDialogEvent =
        new CustomEvent('open-add-print-server-dialog', {
          bubbles: true,
          composed: true,
        });
    this.dispatchEvent(openAddPrintServerDialogEvent);
  }

  private onProtocolChange_(event: Event): void {
    // Queue input should be hidden when protocol is set to "App Socket".
    const selectEl = cast(event.target, HTMLSelectElement);
    this.showPrinterQueue_ = selectEl.value !== 'socket';
    this.set('newPrinter.printerProtocol', selectEl!.value);
  }

  private canAddPrinter_(): boolean {
    return (
        !this.addPrinterInProgress_ && isNameAndAddressValid(this.newPrinter));
  }

  private printerInfoChanged_(): void {
    this.$.printerAddressInput.invalid = false;
    this.errorText_ = '';
  }

  private onKeypress_(event: KeyboardEvent): void {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();

    if (this.canAddPrinter_()) {
      this.addPressed_();
    }
  }

  private recordAddPrinterResult(success: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'Printing.CUPS.AddPrinterManuallyResult', success);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'add-printer-manually-dialog': AddPrinterManuallyDialogElement;
  }
}

customElements.define(
    AddPrinterManuallyDialogElement.is, AddPrinterManuallyDialogElement);
