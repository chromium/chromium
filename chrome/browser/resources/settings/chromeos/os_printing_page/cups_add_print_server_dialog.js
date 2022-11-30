// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-print-server-dialog' is a dialog in which the user can
 *   add a print server.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import './cups_add_printer_dialog.js';
import './cups_printer_dialog_error.js';
import './cups_printer_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getPrintServerErrorText} from './cups_printer_dialog_util.js';
import {CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, CupsPrintersList, PrintServerResult} from './cups_printers_browser_proxy.js';

/** @polymer */
class AddPrintServerDialogElement extends PolymerElement {
  static get is() {
    return 'add-print-server-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {string} */
      printServerAddress_: {
        type: String,
        value: '',
      },

      /** @private {string} */
      errorText_: {
        type: String,
        value: '',
      },

      /** @private {boolean} */
      inProgress_: {
        type: Boolean,
        value: false,
      },

    };
  }

  constructor() {
    super();

    /** @private {!CupsPrintersBrowserProxy} */
    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();
  }

  /** @private */
  onCancelTap_() {
    this.shadowRoot.querySelector('add-printer-dialog').close();
  }

  /** @private */
  onAddPrintServerTap_() {
    this.inProgress_ = true;
    this.shadowRoot.querySelector('#printServerAddressInput').invalid = false;
    this.browserProxy_.queryPrintServer(this.printServerAddress_)
        .then(
            this.onPrintServerAddedSucceeded_.bind(this),
            this.onPrintServerAddedFailed_.bind(this));
  }

  /**
   * @param {!CupsPrintersList} printers
   * @private
   */
  onPrintServerAddedSucceeded_(printers) {
    this.inProgress_ = false;
    const addPrintServerEvent =
        new CustomEvent('add-print-server-and-show-toast', {
          bubbles: true,
          composed: true,
          detail: {printers},
        });
    this.dispatchEvent(addPrintServerEvent);
    this.shadowRoot.querySelector('add-printer-dialog').close();
  }

  /**
   * @param {*} addPrintServerError
   * @private
   */
  onPrintServerAddedFailed_(addPrintServerError) {
    this.inProgress_ = false;
    if (addPrintServerError === PrintServerResult.INCORRECT_URL) {
      this.shadowRoot.querySelector('#printServerAddressInput').invalid = true;
      return;
    }
    this.errorText_ = getPrintServerErrorText(
        /** @type {PrintServerResult} */ (addPrintServerError));
  }

  /**
   * Keypress event handler. If enter is pressed, trigger the add event.
   * @param {!Event} event
   * @private
   */
  onKeypress_(event) {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();

    this.onAddPrintServerTap_();
  }
}

customElements.define(
    AddPrintServerDialogElement.is, AddPrintServerDialogElement);
