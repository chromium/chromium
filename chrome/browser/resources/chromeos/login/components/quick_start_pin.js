// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the PIN on the QuickStart screen
 */


import { assert } from '//resources/ash/common/assert.js';
import { html, PolymerElement } from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class QuickStartPin extends PolymerElement {
  static get is() {
    return 'quick-start-pin';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // PIN provided externally.
      pin: {
        type: String,
        value: '0000',
      },
      // Digits extracted internally.
      digit0_: {
        type: String,
        computed: 'extractDigits(pin, 0)',
      },
      digit1_: {
        type: String,
        computed: 'extractDigits(pin, 1)',
      },
      digit2_: {
        type: String,
        computed: 'extractDigits(pin, 2)',
      },
      digit3_: {
        type: String,
        computed: 'extractDigits(pin, 3)',
      },
    };
  }

  extractDigits(pin, position) {
    assert(pin.length === 4, 'PIN must be 4 digits long!');
    assert(position >= 0 && position <= 3);
    return pin[position];
  }
}

customElements.define(QuickStartPin.is, QuickStartPin);
