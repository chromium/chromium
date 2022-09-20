// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card_frame.js';
import './diagnostics_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './diagnostics_card.html.js';

/**
 * @fileoverview
 * 'diagnostics-card' is a styling wrapper for each component's diagnostic
 * card.
 */

/** @polymer */
export class DiagnosticsCardElement extends PolymerElement {
  static get is() {
    return 'diagnostics-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

    };
  }

  /**
   * @return {string}
   * @protected
   */
  getTopSectionClassName_() {
    return `top-section${this.isNetworkingCard ? '-networking' : ''}`;
  }

  /**
   * @return {string}
   * @private
   */
  getBodyClassName_() {
    return `data-points${this.isNetworkingCard ? '-column' : ''}`;
  }
}

customElements.define(DiagnosticsCardElement.is, DiagnosticsCardElement);
