// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'printer-dialog-error' is the error container for dialogs.
 */
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './cups_printer_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cups_printer_dialog_error.html.js';

export class PrinterDialogErrorElement extends PolymerElement {
  static get is() {
    return 'printer-dialog-error';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The error text to be displayed on the dialog.
       */
      errorText: {
        type: String,
        value: '',
      },
    };
  }

  private errorText: string;
}

declare global {
  interface HTMLElementTagNameMap {
    'printer-dialog-error': PrinterDialogErrorElement;
  }
}

customElements.define(PrinterDialogErrorElement.is, PrinterDialogErrorElement);
