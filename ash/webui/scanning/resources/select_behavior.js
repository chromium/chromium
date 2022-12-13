// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Helper functions for custom elements that implement a scan setting using a
 * single select element. Elements that use this behavior are required to
 * implement getOptionAtIndex(), sortOptions(), and isDefaultOption().
 * @polymerBehavior
 */
export const SelectBehavior = {
  properties: {
    /** @type {boolean} */
    disabled: Boolean,

    /** @type {!Array} */
    options: {
      type: Array,
      value: () => [],
    },

    /** @type {string} */
    selectedOption: {
      type: String,
      notify: true,
    },
  },

  observers: ['onOptionsChange_(options.*)'],

  /**
   * Get the index of the default option if it exists. If not, use the index of
   * the first option in the options array.
   * @return {number}
   * @private
   */
  getDefaultSelectedIndex_() {
    assert(this.options.length > 0);

    const defaultIndex = this.options.findIndex((option) => {
      return this.isDefaultOption(option);
    });

    return defaultIndex === -1 ? 0 : defaultIndex;
  },

  /**
   * Sorts the options and sets the selected option.
   * @private
   */
  onOptionsChange_() {
    if (this.options.length > 1) {
      this.sortOptions();
    }

    if (this.options.length > 0) {
      const selectedOptionIndex = this.getDefaultSelectedIndex_();
      this.selectedOption = this.getOptionAtIndex(selectedOptionIndex);
      this.$$('select').selectedIndex = selectedOptionIndex;
    }
  },
};
