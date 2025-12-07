// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '/strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Destination} from '../data/destination.js';
import {PrinterType} from '../data/destination.js';
import {State} from '../data/state.js';

import {getCss} from './button_strip.css.js';
import {getHtml} from './button_strip.html.js';


export class PrintPreviewButtonStripElement extends CrLitElement {
  static get is() {
    return 'print-preview-button-strip';
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
      firstLoad: {type: Boolean},
      state: {type: Number},
      printButtonEnabled_: {type: Boolean},
      printButtonLabel_: {type: String},
    };
  }

  accessor destination: Destination|null = null;
  accessor firstLoad: boolean = false;
  accessor state: State = State.NOT_READY;
  protected accessor printButtonEnabled_: boolean = false;
  protected accessor printButtonLabel_: string =
      loadTimeData.getString('printButton');
  private lastState_: State = State.NOT_READY;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('destination')) {
      this.printButtonLabel_ =
          loadTimeData.getString(this.isPdf_() ? 'saveButton' : 'printButton');
    }

    if (changedProperties.has('state')) {
      this.updatePrintButtonEnabled_();
    }
  }

  protected onPrintClick_() {
    this.fire('print-requested');
  }

  protected onCancelClick_() {
    this.fire('cancel-requested');
  }

  private isPdf_(): boolean {
    return !!this.destination &&
        this.destination.type === PrinterType.PDF_PRINTER;
  }

  private updatePrintButtonEnabled_() {
    switch (this.state) {
      case (State.PRINTING):
        this.printButtonEnabled_ = false;
        break;
      case (State.READY):
        this.printButtonEnabled_ = true;
        if (this.firstLoad || this.lastState_ === State.PRINTING) {
          this.shadowRoot
              .querySelector<CrButtonElement>(
                  'cr-button.action-button')!.focus();
          this.fire('print-button-focused');
        }
        break;
      default:
        this.printButtonEnabled_ = false;
        break;
    }
    this.lastState_ = this.state;
  }
}

export type ButtonStripElement = PrintPreviewButtonStripElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-button-strip': PrintPreviewButtonStripElement;
  }
}

customElements.define(
    PrintPreviewButtonStripElement.is, PrintPreviewButtonStripElement);
