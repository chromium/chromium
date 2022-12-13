// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_shared_css.js';
import './strings.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
    pageIndex: Number,

    /** @type {number} */
    numTotalPages: Number,

    /** @private {string} */
    pageNumberText_: {
      type: String,
      computed: 'computePageNumberText_(pageIndex, numTotalPages)',
    },
  },

  /**
   * @return {string}
   * @private
   */
  computePageNumberText_() {
    if (!this.numTotalPages || this.pageIndex >= this.numTotalPages) {
      return '';
    }

    assert(this.numTotalPages > 0);
    // Add 1 to |pageIndex| to get the corresponding page number.
    return this.i18n(
        'actionToolbarPageCountText', this.pageIndex + 1, this.numTotalPages);
  },

  /** @private */
  onRemovePageIconClick_() {
    this.fire('show-remove-page-dialog', this.pageIndex);
  },

  /** @private */
  onRescanPageIconClick_() {
    this.fire('show-rescan-page-dialog', this.pageIndex);
  },
});
