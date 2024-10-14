// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  html,
  nothing,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';
import {Duration, formatDuration} from '../core/utils/datetime.js';

/**
 * Component formats time duration, associates corresponding full digital format
 * aria label.
 */
export class TimeDuration extends ReactiveLitElement {
  static override properties: PropertyDeclarations = {
    digits: {type: Number},
    duration: {attribute: false},
  };

  digits = 0;

  duration: Duration|null = null;

  override render(): RenderResult {
    if (this.duration === null) {
      return nothing;
    }
    return html`
      <div aria-label=${formatDuration(this.duration, this.digits, true)}>
        <span aria-hidden="true">
          ${formatDuration(this.duration, this.digits)}
        </span>
      </div>
    `;
  }
}

window.customElements.define('time-duration', TimeDuration);

declare global {
  interface HTMLElementTagNameMap {
    'time-duration': TimeDuration;
  }
}
