// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {alphabeticalCompare, getPageSizeString} from './scanning_app_util.js';
import {SelectBehavior} from './select_behavior.js';

/** @type {ash.scanning.mojom.PageSize} */
const DEFAULT_PAGE_SIZE = ash.scanning.mojom.PageSize.kNaLetter;

/**
 * @fileoverview
 * 'page-size-select' displays the available page sizes in a dropdown.
 */
Polymer({
  is: 'page-size-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  /**
   * @param {number} index
   * @return {string}
   */
  getOptionAtIndex(index) {
    assert(index < this.options.length);

    return this.options[index].toString();
  },

  /**
   * @param {!ash.scanning.mojom.PageSize} pageSize
   * @return {string}
   * @private
   */
  getPageSizeString_(pageSize) {
    return getPageSizeString(pageSize);
  },

  sortOptions() {
    this.options.sort((a, b) => {
      return alphabeticalCompare(getPageSizeString(a), getPageSizeString(b));
    });

    // If the fit to scan area option exists, move it to the end of the page
    // sizes array.
    const fitToScanAreaIndex = this.options.findIndex((pageSize) => {
      return pageSize === ash.scanning.mojom.PageSize.kMax;
    });


    if (fitToScanAreaIndex !== -1) {
      this.options.push(this.options.splice(fitToScanAreaIndex, 1)[0]);
    }
  },

  /**
   * @param {!ash.scanning.mojom.PageSize} option
   * @return {boolean}
   */
  isDefaultOption(option) {
    return option === DEFAULT_PAGE_SIZE;
  },
});
