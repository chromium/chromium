// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './scanning_fonts_css.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppState} from './scanning_app_types.js';
import {ScanningBrowserProxyImpl, SelectedPath} from './scanning_browser_proxy.js';

/**
 * @fileoverview
 * 'multi-page-scan' shows the available actions for a multi-page scan.
 */
Polymer({
  is: 'multi-page-scan',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!AppState} */
    appState: {
      type: Number,
      observer: 'onAppStateChange_',
    },

    /** @type {number} */
    pageNumber: {
      type: Number,
      observer: 'onPageNumberChange_',
    },

    /** @private {string} */
    scanButtonText_: String,

    /** @private {boolean} */
    showCancelButton_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    cancelButtonDisabled_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showCancelingText_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onAppStateChange_() {
    this.showCancelButton_ = this.appState === AppState.MULTI_PAGE_SCANNING ||
        this.appState === AppState.MULTI_PAGE_CANCELING;
    this.cancelButtonDisabled_ =
        this.appState === AppState.MULTI_PAGE_CANCELING;
    this.showCancelingText_ = this.appState === AppState.MULTI_PAGE_CANCELING;
  },

  /** @private */
  onPageNumberChange_() {
    ScanningBrowserProxyImpl.getInstance()
        .getPluralString('scanButtonText', this.pageNumber + 1)
        .then(
            /* @type {string} */ (pluralString) => {
              this.scanButtonText_ = pluralString;
            });
  },

  /** @private */
  onScanClick_() {
    this.fire('scan-next-page');
  },

  /** @private */
  onSaveClick_() {
    this.fire('complete-multi-page-scan');
  },

  /** @private */
  onCancelClick_() {
    this.fire('cancel-click');
  },

  /**
   * @return {string}
   * @private
   */
  getProgressText_() {
    return this.i18n('multiPageScanProgressText', this.pageNumber);
  },
});
