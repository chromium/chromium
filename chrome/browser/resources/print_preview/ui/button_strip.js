// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {getPrinterTypeForDestination, PrinterType} from '../data/destination_match.js';
import {State} from '../data/state.js';

Polymer({
  is: 'print-preview-button-strip',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!Destination} */
    destination: Object,

    firstLoad: Boolean,

    maxSheets: Number,

    sheetCount: Number,

    /** @type {!State} */
    state: Number,

    /** @private */
    printButtonEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    printButtonLabel_: {
      type: String,
      value() {
        return loadTimeData.getString('printButton');
      },
    },

    // <if expr="chromeos">
    /** @private */
    errorMessage_: {
      type: String,
      observer: 'errorMessageChanged_',
    },
    // </if>
  },

  observers: [
    'updatePrintButtonLabel_(destination.id)',
    'updatePrintButtonEnabled_(state, destination.id, maxSheets, sheetCount)',
    // <if expr="chromeos">
    'updateErrorMessage_(state, destination.id, maxSheets, sheetCount)',
    // </if>
  ],

  /** @private {!State} */
  lastState_: State.NOT_READY,

  /** @private */
  onPrintClick_() {
    this.fire('print-requested');
  },

  /** @private */
  onCancelClick_() {
    this.fire('cancel-requested');
  },

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
  updatePrintButtonLabel_() {
    this.printButtonLabel_ = loadTimeData.getString(
        this.isPdfOrDrive_() ? 'saveButton' : 'printButton');
  },

  /** @private */
  updatePrintButtonEnabled_() {
    switch (this.state) {
      case (State.PRINTING):
        this.printButtonEnabled_ = false;
        break;
      case (State.READY):
        // <if expr="chromeos">
        this.printButtonEnabled_ = !this.printButtonDisabled_();
        // </if>
        // <if expr="not chromeos">
        this.printButtonEnabled_ = true;
        // </if>
        if (this.firstLoad) {
          this.$$('cr-button.action-button').focus();
          this.fire('print-button-focused');
        }
        break;
      default:
        this.printButtonEnabled_ = false;
        break;
    }
    this.lastState_ = this.state;
  },

  // <if expr="chromeos">
  /**
   * @return {boolean} Whether to disable "Print" button because of sheets limit
   *     policy.
   * @private
   */
  printButtonDisabled_() {
    // The "Print" button is disabled if 3 conditions are met:
    // * This is "real" printing, i.e. not saving to PDF/Drive.
    // * Sheets policy is present.
    // * Either number of sheets is not calculated or exceeds policy limit.
    return !this.isPdfOrDrive_() && this.maxSheets > 0 &&
        (this.sheetCount === 0 || this.sheetCount > this.maxSheets);
  },

  /**
   * @return {boolean} Whether to show the "Too many sheets" error.
   * @private
   */
  showSheetsError_() {
    // The error is shown if the number of sheets is already calculated and the
    // print button is disabled.
    return this.sheetCount > 0 && this.printButtonDisabled_();
  },

  /** @private */
  updateErrorMessage_() {
    if (!this.showSheetsError_()) {
      this.errorMessage_ = '';
      return;
    }
    PluralStringProxyImpl.getInstance()
        .getPluralString('sheetsLimitErrorMessage', this.maxSheets)
        .then(label => {
          this.errorMessage_ = label;
        });
  },

  /**
   * Uses IronA11yAnnouncer to notify screen readers that an error is set.
   * @private
   */
  errorMessageChanged_() {
    if (this.errorMessage_ !== '') {
      IronA11yAnnouncer.requestAvailability();
      this.fire('iron-announce', {text: this.errorMessage_});
    }
  },
  // </if>
});
