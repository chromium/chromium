// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';
import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Destination} from '../data/destination.js';
import {PrinterType} from '../data/destination.js';
import {Error, State} from '../data/state.js';

import {getCss} from './header.css.js';
import {getHtml} from './header.html.js';
import {SettingsMixin} from './settings_mixin.js';


const PrintPreviewHeaderElementBase = SettingsMixin(CrLitElement);

export class PrintPreviewHeaderElement extends PrintPreviewHeaderElementBase {
  static get is() {
    return 'print-preview-header';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      destination: {type: Object},
      error: {type: Number},
      state: {type: Number},
      managed: {type: Boolean},
      sheetCount_: {type: Number},
      summary_: {type: String},
    };
  }

  accessor destination: Destination|null = null;
  accessor error: Error|null = null;
  accessor state: State = State.NOT_READY;
  accessor managed: boolean = false;
  private accessor sheetCount_: number = 0;
  protected accessor summary_: string|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.addSettingObserver('pages.*', this.updateSheetCount_.bind(this));
    this.addSettingObserver('duplex.*', this.updateSheetCount_.bind(this));
    this.addSettingObserver('copies.*', this.updateSheetCount_.bind(this));
    this.updateSheetCount_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('sheetCount_') ||
        changedProperties.has('state') ||
        changedProperties.has('destination')) {
      this.updateSummary_();
    }
  }

  private updateSheetCount_() {
    let sheets = (this.getSettingValue('pages') as number[]).length;
    if (this.getSettingValue('duplex')) {
      sheets = Math.ceil(sheets / 2);
    }
    this.sheetCount_ = sheets * (this.getSettingValue('copies') as number);
  }

  private isPdf_(): boolean {
    return !!this.destination &&
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
    if (this.sheetCount_ === 0) {
      this.summary_ = '';
      return;
    }

    const pageOrSheet = this.isPdf_() ? 'Page' : 'Sheet';
    PluralStringProxyImpl.getInstance()
        .getPluralString(
            `printPreview${pageOrSheet}SummaryLabel`, this.sheetCount_)
        .then(label => {
          this.summary_ = label;
        });
  }
}

export type HeaderElement = PrintPreviewHeaderElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-header': PrintPreviewHeaderElement;
  }
}

customElements.define(PrintPreviewHeaderElement.is, PrintPreviewHeaderElement);
