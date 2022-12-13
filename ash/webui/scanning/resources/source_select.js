// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {alphabeticalCompare, getSourceTypeString} from './scanning_app_util.js';
import {SelectBehavior} from './select_behavior.js';

/** @type {ash.scanning.mojom.SourceType} */
const DEFAULT_SOURCE_TYPE = ash.scanning.mojom.SourceType.kFlatbed;

/**
 * @fileoverview
 * 'source-select' displays the available scanner sources in a dropdown.
 */
Polymer({
  is: 'source-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  /**
   * @param {number} index
   * @return {string}
   */
  getOptionAtIndex(index) {
    assert(index < this.options.length);

    return this.options[index].name;
  },

  /**
   * @param {ash.scanning.mojom.SourceType} mojoSourceType
   * @return {string}
   * @private
   */
  getSourceTypeString_(mojoSourceType) {
    return getSourceTypeString(mojoSourceType);
  },

  sortOptions() {
    this.options.sort((a, b) => {
      return alphabeticalCompare(
          getSourceTypeString(a.type), getSourceTypeString(b.type));
    });
  },

  /**
   * @param {!ash.scanning.mojom.ScanSource} option
   * @return {boolean}
   */
  isDefaultOption(option) {
    return option.type === DEFAULT_SOURCE_TYPE;
  },
});
