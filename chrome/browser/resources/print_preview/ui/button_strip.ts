// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '/strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';
import {PrinterType} from '../data/destination.js';
import {State} from '../data/state.js';

import {getTemplate} from './button_strip.html.js';


export class PrintPreviewButtonStripElement extends PolymerElement {
  static get is() {
    return 'print-preview-button-strip';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      destination: Object,

      firstLoad: Boolean,

      maxSheets: Number,

      sheetCount: Number,

      state: Number,

      printButtonEnabled_: {
        type: Boolean,
        value: false,
      },

      printButtonLabel_: {
        type: String,
        value() {
          return loadTimeData.getString('printButton');
        },
      },
    };
  }

  static get observers() {
    return [
      'updatePrintButtonLabel_(destination.id)',
      'updatePrintButtonEnabled_(state, destination.id, maxSheets, sheetCount)',
    ];
  }

  destination: Destination;
  firstLoad: boolean;
  maxSheets: number;
  sheetCount: number;
  state: State;
  private printButtonEnabled_: boolean;
  private printButtonLabel_: string;
  private lastState_: State = State.NOT_READY;

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private onPrintClick_() {
    this.fire_('print-requested');
  }

  private onCancelClick_() {
    this.fire_('cancel-requested');
  }

  private isPdf_(): boolean {
    return this.destination &&
        this.destination.type === PrinterType.PDF_PRINTER;
  }

  private updatePrintButtonLabel_() {
    this.printButtonLabel_ =
        loadTimeData.getString(this.isPdf_() ? 'saveButton' : 'printButton');
  }

  private updatePrintButtonEnabled_() {
    switch (this.state) {
      case (State.PRINTING):
        this.printButtonEnabled_ = false;
        break;
      case (State.READY):
        this.printButtonEnabled_ = true;
        if (this.firstLoad || this.lastState_ === State.PRINTING) {
          this.shadowRoot!
              .querySelector<CrButtonElement>(
                  'cr-button.action-button')!.focus();
          this.fire_('print-button-focused');
        }
        break;
      default:
        this.printButtonEnabled_ = false;
        break;
    }
    this.lastState_ = this.state;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-button-strip': PrintPreviewButtonStripElement;
  }
}

customElements.define(
    PrintPreviewButtonStripElement.is, PrintPreviewButtonStripElement);
