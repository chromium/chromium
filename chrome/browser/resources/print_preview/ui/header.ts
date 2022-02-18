// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';
import './print_preview_vars_css.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, GooglePromotedDestinationId} from '../data/destination.js';
import {getPrinterTypeForDestination, PrinterType} from '../data/destination_match.js';
import {Error, State} from '../data/state.js';

import {getTemplate} from './header.html.js';
import {SettingsMixin} from './settings_mixin.js';


const PrintPreviewHeaderElementBase = SettingsMixin(PolymerElement);

export class PrintPreviewHeaderElement extends PrintPreviewHeaderElementBase {
  static get is() {
    return 'print-preview-header';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      cloudPrintErrorMessage: String,

      destination: Object,

      error: Number,

      state: Number,

      managed: Boolean,

      sheetCount: Number,

      summary_: String,
    };
  }

  static get observers() {
    return [
      'updateSummary_(sheetCount, state, destination.id)',
    ];
  }

  cloudPrintErrorMessage: string;
  destination: Destination;
  error: Error;
  state: State;
  managed: boolean;
  sheetCount: number;
  private summary_: string|null;

  private isPdfOrDrive_(): boolean {
    return this.destination &&
        (getPrinterTypeForDestination(this.destination) ===
             PrinterType.PDF_PRINTER ||
         this.destination.id === GooglePromotedDestinationId.DOCS);
  }

  private updateSummary_() {
    switch (this.state) {
      case (State.PRINTING):
        this.summary_ = loadTimeData.getString(
            this.isPdfOrDrive_() ? 'saving' : 'printing');
        break;
      case (State.READY):
        this.updateSheetsSummary_();
        break;
      case (State.FATAL_ERROR):
        this.summary_ = this.getErrorMessage_();
        break;
      default:
        this.summary_ = null;
        break;
    }
  }

  /**
   * @return The error message to display.
   */
  private getErrorMessage_(): string {
    switch (this.error) {
      case Error.PRINT_FAILED:
        return loadTimeData.getString('couldNotPrint');
      case Error.CLOUD_PRINT_ERROR:
        return this.cloudPrintErrorMessage;
      default:
        return '';
    }
  }

  private updateSheetsSummary_() {
    if (this.sheetCount === 0) {
      this.summary_ = '';
      return;
    }

    const pageOrSheet = this.isPdfOrDrive_() ? 'Page' : 'Sheet';
    PluralStringProxyImpl.getInstance()
        .getPluralString(
            `printPreview${pageOrSheet}SummaryLabel`, this.sheetCount)
        .then(label => {
          this.summary_ = label;
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-header': PrintPreviewHeaderElement;
  }
}

customElements.define(PrintPreviewHeaderElement.is, PrintPreviewHeaderElement);
