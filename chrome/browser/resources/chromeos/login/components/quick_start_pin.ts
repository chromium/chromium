// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the PIN on the QuickStart screen
 */

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './quick_start_pin.html.js';

class QuickStartPin extends PolymerElement {
  static get is() {
    return 'quick-start-pin' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      // PIN provided externally.
      pin: {
        type: String,
        value: '0000',
      },
      // Digits extracted internally.
      digit0: {
        type: String,
        computed: 'extractDigits(pin, 0)',
      },
      digit1: {
        type: String,
        computed: 'extractDigits(pin, 1)',
      },
      digit2: {
        type: String,
        computed: 'extractDigits(pin, 2)',
      },
      digit3: {
        type: String,
        computed: 'extractDigits(pin, 3)',
      },
    };
  }

  private pin: string;
  private digit0: string;
  private digit1: string;
  private digit2: string;
  private digit3: string;

  private extractDigits(pin: string, position: number): string {
    assert(pin.length === 4, 'PIN must be 4 digits long!');
    assert(position >= 0 && position <= 3);
    return pin[position];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [QuickStartPin.is]: QuickStartPin;
  }
}

customElements.define(QuickStartPin.is, QuickStartPin);
