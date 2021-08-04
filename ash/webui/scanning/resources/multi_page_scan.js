// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './scanning_fonts_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
    /** @type {number} */
    pageNumber: {
      type: Number,
      observer: 'onPageNumberChange_',
    },

    /** @private {string} */
    scanButtonText_: String,
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
});
