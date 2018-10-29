// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This element contains a set of SVGs that together acts as an
 * animated and responsive background for any page that contains it.
 */
Polymer({
  is: 'step-indicator',

  properties: {
    /** @type {nux.stepIndicatorModel} */
    model: Object,

    /** @private */
    dots_: {
      type: Array,
      computed: 'computeDots_(model.total)',
    },
  },

  /**
   * @return {!Array<undefined>}
   * @private
   */
  computeDots_: function() {
    // If total is 1, show nothing.
    return new Array(this.model.total > 1 ? this.model.total : 0);
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getActiveClass_: function(index) {
    return index == this.model.active ? 'active' : '';
  },
});