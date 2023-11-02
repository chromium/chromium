// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrScrollableBehavior} from 'chrome://resources/ash/common/cr_scrollable_behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'edu-coexistence-template',

  _template: html`{__html_template__}`,

  behaviors: [CrScrollableBehavior],

  properties: {
    /**
     * Indicates whether the footer/button div should be shown.
     * @private {boolean}
     */
    showButtonFooter_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Shows/hides the button footer.
   * @param {boolean} show Whether to show the footer.
   */
  showButtonFooter(show) {
    this.showButtonFooter_ = show;
  },

});
