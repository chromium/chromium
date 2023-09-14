// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './page_size_select.html.js';
import {PageSize} from './scanning.mojom-webui.js';
import {alphabeticalCompare, getPageSizeString} from './scanning_app_util.js';
import {SelectBehavior, SelectBehaviorInterface} from './select_behavior.js';

/** @type {PageSize} */
const DEFAULT_PAGE_SIZE = PageSize.kNaLetter;

/**
 * @fileoverview
 * 'page-size-select' displays the available page sizes in a dropdown.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {SelectBehaviorInterface}
 */
const PageSizeSelectElementBase =
    mixinBehaviors([I18nBehavior, SelectBehavior], PolymerElement);

/** @polymer */
class PageSizeSelectElement extends PageSizeSelectElementBase {
  static get is() {
    return 'page-size-select';
  }

  static get template() {
    return getTemplate();
  }

  /**
   * @param {number} index
   * @return {string}
   */
  getOptionAtIndex(index) {
    assert(index < this.options.length);

    return this.options[index].toString();
  }

  /**
   * @param {!PageSize} pageSize
   * @return {string}
   * @private
   */
  getPageSizeString_(pageSize) {
    return getPageSizeString(pageSize);
  }

  sortOptions() {
    this.options.sort((a, b) => {
      return alphabeticalCompare(getPageSizeString(a), getPageSizeString(b));
    });

    // If the fit to scan area option exists, move it to the end of the page
    // sizes array.
    const fitToScanAreaIndex = this.options.findIndex((pageSize) => {
      return pageSize === PageSize.kMax;
    });


    if (fitToScanAreaIndex !== -1) {
      this.options.push(this.options.splice(fitToScanAreaIndex, 1)[0]);
    }
  }

  /**
   * @param {!PageSize} option
   * @return {boolean}
   */
  isDefaultOption(option) {
    return option === DEFAULT_PAGE_SIZE;
  }
}

customElements.define(PageSizeSelectElement.is, PageSizeSelectElement);
