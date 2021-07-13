// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_shared_css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ScanningBrowserProxy, ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

/**
 * @fileoverview
 * 'action-toolbar' is a floating toolbar that contains post-scan page options.
 */
Polymer({
  is: 'action-toolbar',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {number} */
    currentPageInView: Number,

    /** @type {number} */
    numTotalPages: Number,

    /** @private {string} */
    pageNumberText_: {
      type: String,
      computed: 'computePageNumberText_(currentPageInView, numTotalPages)',
    },

    /** @private {string} */
    removeButtonTooltip_: String,

    /** @private {string} */
    rescanButtonTooltip_: String,
  },

  /** @override */
  ready() {
    /** @type {!ScanningBrowserProxy} */
    const browserProxy = ScanningBrowserProxyImpl.getInstance();
    browserProxy.getPluralString('removePageButtonLabel', 0)
        .then(/* @type {string} */ (pluralString) => {
          this.removeButtonTooltip_ = pluralString;
        });
    browserProxy.getPluralString('rescanPageButtonLabel', 0)
        .then(
            /* @type {string} */ (pluralString) => {
              this.rescanButtonTooltip_ = pluralString;
            });
  },

  /**
   * @return {string}
   * @private
   */
  computePageNumberText_() {
    if (!this.currentPageInView || !this.numTotalPages) {
      return '';
    }

    assert(this.currentPageInView > 0 && this.numTotalPages > 0);
    assert(this.currentPageInView <= this.numTotalPages);

    return this.i18n(
        'actionToolbarPageCountText', this.currentPageInView,
        this.numTotalPages);
  },
});
