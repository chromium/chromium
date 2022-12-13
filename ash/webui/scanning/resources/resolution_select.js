// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SelectBehavior} from './select_behavior.js';

/** @type {number} */
const DEFAULT_RESOLUTION = 300;

/**
 * @fileoverview
 * 'resolution-select' displays the available scan resolutions in a dropdown.
 */
Polymer({
  is: 'resolution-select',

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
   * @param {number} resolution
   * @return {string}
   * @private
   */
  getResolutionString_(resolution) {
    return this.i18n('resolutionOptionText', resolution);
  },

  sortOptions() {
    // Sort the resolutions in descending order.
    this.options.sort(function(a, b) {
      return b - a;
    });
  },

  /**
   * @param {number} option
   * @return {boolean}
   */
  isDefaultOption(option) {
    return option === DEFAULT_RESOLUTION;
  },
});
