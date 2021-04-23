// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'printer-dialog-error' is the error container for dialogs.
 */
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './cups_printer_shared_css.js';

import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getBaseName, getErrorText, getPrintServerErrorText, isNameAndAddressValid, isNetworkProtocol, isPPDInfoValid, matchesSearchTerm, sortPrinters} from './cups_printer_dialog_util.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, CupsPrintersList, ManufacturersInfo, ModelsInfo, PrinterMakeModel, PrinterPpdMakeModel, PrinterSetupResult, PrintServerResult} from './cups_printers_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'printer-dialog-error',

  properties: {
    /** The error text to be displayed on the dialog. */
    errorText: {
      type: String,
      value: '',
    },
  },
});
