// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'printer-dialog-error' is the error container for dialogs.
 */
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './cups_printer_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class PrinterDialogErrorElement extends PolymerElement {
  static get is() {
    return 'printer-dialog-error';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** The error text to be displayed on the dialog. */
      errorText: {
        type: String,
        value: '',
      },
    };
  }
}

customElements.define(PrinterDialogErrorElement.is, PrinterDialogErrorElement);
