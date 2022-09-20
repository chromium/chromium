// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card_frame.js';
import './diagnostics_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
    return html`{__html_template__}`;
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
