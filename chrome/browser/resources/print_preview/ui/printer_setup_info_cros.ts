// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './print_preview_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './printer_setup_info_cros.html.js';

/**
 * @fileoverview PrintPreviewPrinterSetupInfoCrosElement
 * This element provides contextual instructions to help users navigate
 * to printer settings based on the state of printers available in
 * print-preview. Element will use NativeLayer to open the correct printer
 * settings interface.
 */

export class PrintPreviewPrinterSetupInfoCrosElement extends PolymerElement {
  static get is() {
    return 'print-preview-printer-setup-info-cros' as const;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PrintPreviewPrinterSetupInfoCrosElement.is]:
        PrintPreviewPrinterSetupInfoCrosElement;
  }
}

customElements.define(
    PrintPreviewPrinterSetupInfoCrosElement.is,
    PrintPreviewPrinterSetupInfoCrosElement);
