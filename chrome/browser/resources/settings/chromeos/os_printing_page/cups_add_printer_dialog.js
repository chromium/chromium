// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-printer-dialog' is the template of the Add Printer
 * dialog.
 */
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './cups_printer_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class AddPrinterDialogElement extends PolymerElement {
  static get is() {
    return 'add-printer-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @private */
  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  close() {
    this.$.dialog.close();
  }
}

customElements.define(AddPrinterDialogElement.is, AddPrinterDialogElement);
