// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-printer-dialog' is the template of the Add Printer
 * dialog.
 */
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './cups_printer_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cups_add_printer_dialog.html.js';

export interface AddPrinterDialogElement {
  $: {dialog: CrDialogElement};
}

export class AddPrinterDialogElement extends PolymerElement {
  static get is() {
    return 'add-printer-dialog';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  close(): void {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'add-printer-dialog': AddPrinterDialogElement;
  }
}

customElements.define(AddPrinterDialogElement.is, AddPrinterDialogElement);
