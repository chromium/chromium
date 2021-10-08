// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card_frame.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'diagnostics-card' is a styling wrapper for each component's diagnostic
 * card.
 */
Polymer({
  is: 'diagnostics-card',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {boolean} */
    hideDataPoints: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @type {boolean} */
    isNetworkingCard: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /**
   * @return {string}
   * @protected
   */
  getTopSectionClassName_() {
    return `top-section${this.isNetworkingCard ? '-networking' : ''}`;
  },

  /**
   * @return {string}
   * @private
   */
  getBodyClassName_() {
    return `data-points${this.isNetworkingCard ? '-column' : ''}`;
  },
});
