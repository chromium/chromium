// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-print-server-dialog' is a dialog in which the user can
 *   add a print server.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import './cups_add_printer_dialog.js';
import './cups_printer_dialog_error.js';
import './cups_printer_shared_css.js';

import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import {getBaseName, getErrorText, getPrintServerErrorText, isNameAndAddressValid, isNetworkProtocol, isPPDInfoValid, matchesSearchTerm, sortPrinters} from './cups_printer_dialog_util.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, CupsPrintersList, ManufacturersInfo, ModelsInfo, PrinterMakeModel, PrinterPpdMakeModel, PrinterSetupResult, PrintServerResult} from './cups_printers_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'add-print-server-dialog',

  properties: {
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
  },

  /** @private */
  onCancelTap_: function() {
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  onAddPrintServerTap_: function() {
    this.inProgress_ = true;
    this.$$('#printServerAddressInput').invalid = false;
    CupsPrintersBrowserProxyImpl.getInstance()
        .queryPrintServer(this.printServerAddress_)
        .then(
            this.onPrintServerAddedSucceeded_.bind(this),
            this.onPrintServerAddedFailed_.bind(this));
  },

  /**
   * @param {!CupsPrintersList} printers
   * @private
   */
  onPrintServerAddedSucceeded_: function(printers) {
    this.inProgress_ = false;
    this.fire('add-print-server-and-show-toast', {printers: printers});
    this.$$('add-printer-dialog').close();
  },

  /**
   * @param {*} addPrintServerError
   * @private
   */
  onPrintServerAddedFailed_: function(addPrintServerError) {
    this.inProgress_ = false;
    if (addPrintServerError === PrintServerResult.INCORRECT_URL) {
      this.$$('#printServerAddressInput').invalid = true;
      return;
    }
    this.errorText_ = getPrintServerErrorText(
        /** @type {PrintServerResult} */ (addPrintServerError));
  },

  /**
   * Keypress event handler. If enter is pressed, trigger the add event.
   * @param {!Event} event
   * @private
   */
  onKeypress_: function(event) {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();

    this.onAddPrintServerTap_();
  },
});
