// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {State} from '../data/state.js';

Polymer({
  is: 'print-preview-button-strip',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!Destination} */
    destination: Object,

    firstLoad: Boolean,

    /** @type {!State} */
    state: {
      type: Number,
      observer: 'updatePrintButtonEnabled_',
    },

    /** @private */
    printButtonEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    printButtonLabel_: {
      type: String,
      value: function() {
        return loadTimeData.getString('printButton');
      },
    },
  },

  observers: ['updatePrintButtonLabel_(destination.id)'],

  /** @private {!State} */
  lastState_: State.NOT_READY,

  /** @private */
  onPrintClick_: function() {
    this.fire('print-requested');
  },

  /** @private */
  onCancelClick_: function() {
    this.fire('cancel-requested');
  },

  /**
   * @return {boolean}
   * @private
   */
  isPdfOrDrive_: function() {
    return this.destination &&
        (this.destination.id == Destination.GooglePromotedId.SAVE_AS_PDF ||
         this.destination.id == Destination.GooglePromotedId.DOCS);
  },

  /** @private */
  updatePrintButtonLabel_: function() {
    this.printButtonLabel_ = loadTimeData.getString(
        this.isPdfOrDrive_() ? 'saveButton' : 'printButton');
  },

  /** @private */
  updatePrintButtonEnabled_: function() {
    switch (this.state) {
      case (State.PRINTING):
        this.printButtonEnabled_ = false;
        break;
      case (State.READY):
        this.printButtonEnabled_ = true;
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
});
