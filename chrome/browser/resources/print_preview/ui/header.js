// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';
import './print_preview_vars_css.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {Error, State} from '../data/state.js';

import {SettingsBehavior} from './settings_behavior.js';

/**
 * @typedef {{numPages: number,
 *            numSheets: number,
 *            pagesLabel: string,
 *            summaryLabel: string}}
 */
let LabelInfo;

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

    /** @private {?string} */
    summary_: {
      type: String,
      value: null,
    },
  },

  observers: [
    'update_(settings.copies.value, settings.duplex.value, ' +
        'settings.pages.value, state, destination.id)',
  ],

  /**
   * @return {boolean}
   * @private
   */
  isPdfOrDrive_: function() {
    return this.destination &&
        (this.destination.id == Destination.GooglePromotedId.SAVE_AS_PDF ||
         this.destination.id == Destination.GooglePromotedId.DOCS);
  },

  /**
   * @return {!LabelInfo}
   * @private
   */
  computeLabelInfo_: function() {
    const saveToPdfOrDrive = this.isPdfOrDrive_();
    let numPages = this.getSettingValue('pages').length;
    let numSheets = numPages;
    if (!saveToPdfOrDrive && this.getSettingValue('duplex')) {
      numSheets = Math.ceil(numPages / 2);
    }

    const copies = parseInt(this.getSettingValue('copies'), 10);
    numSheets *= copies;
    numPages *= copies;

    const pagesLabel = loadTimeData.getString('printPreviewPageLabelPlural');
    let summaryLabel;
    if (numSheets > 1) {
      summaryLabel = saveToPdfOrDrive ?
          pagesLabel :
          loadTimeData.getString('printPreviewSheetsLabelPlural');
    } else {
      summaryLabel = loadTimeData.getString(
          saveToPdfOrDrive ? 'printPreviewPageLabelSingular' :
                             'printPreviewSheetsLabelSingular');
    }
    return {
      numPages: numPages,
      numSheets: numSheets,
      pagesLabel: pagesLabel,
      summaryLabel: summaryLabel
    };
  },

  /** @private */
  update_: function() {
    switch (this.state) {
      case (State.PRINTING):
        this.summary_ = loadTimeData.getString(
            this.isPdfOrDrive_() ? 'saving' : 'printing');
        break;
      case (State.READY):
        const labelInfo = this.computeLabelInfo_();
        this.summary_ = this.getSummary_(labelInfo);
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
  getErrorMessage_: function() {
    switch (this.error) {
      case Error.PRINT_FAILED:
        return loadTimeData.getString('couldNotPrint');
      case Error.CLOUD_PRINT_ERROR:
        return this.cloudPrintErrorMessage;
      default:
        return '';
    }
  },

  /**
   * @param {!LabelInfo} labelInfo
   * @return {string}
   * @private
   */
  getSummary_: function(labelInfo) {
    return labelInfo.numSheets === 0 ?
        '' :
        loadTimeData.getStringF(
            'printPreviewNewSummaryFormatShort',
            labelInfo.numSheets.toLocaleString(), labelInfo.summaryLabel);
  },
});
