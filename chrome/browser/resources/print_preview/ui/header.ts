// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';
import './print_preview_vars.css.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';
import {PrinterType} from '../data/destination.js';
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

  destination: Destination;
  error: Error;
  state: State;
  managed: boolean;
  sheetCount: number;
  private summary_: string|null;

  private isPdf_(): boolean {
    return this.destination &&
        this.destination.type === PrinterType.PDF_PRINTER;
  }

  private updateSummary_() {
    switch (this.state) {
      case (State.PRINTING):
        this.summary_ =
            loadTimeData.getString(this.isPdf_() ? 'saving' : 'printing');
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
      default:
        return '';
    }
  }

  private updateSheetsSummary_() {
    if (this.sheetCount === 0) {
      this.summary_ = '';
      return;
    }

    const pageOrSheet = this.isPdf_() ? 'Page' : 'Sheet';
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
