// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './print_management_fonts.css.js';
import './print_management_shared.css.js';
import './icons.html.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getPrintManagementHandler} from './mojo_interface_provider.js';
import {getTemplate} from './printer_setup_info.html.js';
import {LaunchSource, PrintManagementHandlerInterface} from './printing_manager.mojom-webui.js';

/**
 * @fileoverview
 * 'printer-setup-info' provides messaging on printer setup when there are no
 * print jobs to display.
 */

const PrinterSetupInfoElementIs = 'printer-setup-info';

const PrinterSetupInfoElementBase = I18nMixin(PolymerElement);

export class PrinterSetupInfoElement extends PrinterSetupInfoElementBase {
  static get is(): string {
    return PrinterSetupInfoElementIs;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  private pageHandler: PrintManagementHandlerInterface;

  constructor() {
    super();
    this.pageHandler = getPrintManagementHandler();
  }

  onManagePrintersClicked(): void {
    this.pageHandler.launchPrinterSettings(LaunchSource.kEmptyStateButton);
  }
}

customElements.define(PrinterSetupInfoElement.is, PrinterSetupInfoElement);
