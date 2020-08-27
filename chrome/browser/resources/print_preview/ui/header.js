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
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {getPrinterTypeForDestination, PrinterType} from '../data/destination_match.js';
import {Error, State} from '../data/state.js';
import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-header',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior],

  properties: {
    cloudPrintErrorMessage: String,

    /** @type {!Destination} */
    destination: Object,

    /** @type {!Error} */
    error: Number,

    /** @type {!State} */
    state: Number,

    managed: Boolean,

    sheetCount: Number,

    /** @private {?string} */
    summary_: String,
  },

  observers: [
    'updateSummary_(sheetCount, state, destination.id)',
  ],

  /**
   * @return {boolean}
   * @private
   */
  isPdfOrDrive_() {
    return this.destination &&
        (getPrinterTypeForDestination(this.destination) ===
             PrinterType.PDF_PRINTER ||
         this.destination.id === Destination.GooglePromotedId.DOCS);
  },

  /** @private */
  updateSummary_() {
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
  },

  /**
   * @return {string} The error message to display.
   * @private
   */
  getErrorMessage_() {
    switch (this.error) {
      case Error.PRINT_FAILED:
        return loadTimeData.getString('couldNotPrint');
      case Error.CLOUD_PRINT_ERROR:
        return this.cloudPrintErrorMessage;
      default:
        return '';
    }
  },

  /** @private */
  updateSheetsSummary_() {
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
  },
});
