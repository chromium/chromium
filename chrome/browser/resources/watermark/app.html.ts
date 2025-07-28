// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WatermarkAppElement} from './app.js';

// TODO(crbug.com/434714853): Replace with i18n strings

export function getHtml(this: WatermarkAppElement) {
  return html`
    <div class="controls-card">
      <div class="card-header">
        <div class="header-text">
          <h2>Watermark testing</h2>
          <p>Customize the watermark style and test it live</p>
        </div>
        <cr-button @click="${this.onCopyJsonClick_}">
            Copy style in JSON
        </cr-button>
      </div>

      <div class="control-row">
        <span>Font size</span>
        <cr-input id="fontSizeInput" class="font-size-input
            stroked" type="number"
            min="1" .value="${this.fontSize_.toString()}"
            @value-changed="${this.onFontSizeChanged_}">
        </cr-input>
      </div>

      <div class="control-row">
        <span>White outline opacity</span>
        <cr-slider id="outlineOpacitySlider" min="0" max="100"
            .value="${this.outlineOpacity_}"
            .ticks="${this.opacityTicks_}"
            @cr-slider-value-changed="${this.onOutlineOpacityChanged_}">
        </cr-slider>
      </div>

      <div class="control-row">
        <span>Dark fill opacity</span>
        <cr-slider id="fillOpacitySlider" min="0" max="100"
            .value="${this.fillOpacity_}"
            .ticks="${this.opacityTicks_}"
            @cr-slider-value-changed="${this.onFillOpacityChanged_}">
        </cr-slider>
      </div>
    </div>
  `;
}
