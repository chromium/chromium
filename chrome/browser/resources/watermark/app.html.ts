// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {FONT_SIZE_MAX, FONT_SIZE_MIN, type WatermarkAppElement} from './app.js';

// TODO(crbug.com/434714853): Replace with i18n strings

export function getHtml(this: WatermarkAppElement) {
  return html`
    <div class="controls-card">
      <div class="card-header">
        <div class="header-text">
          <h1>Watermark testing</h1>
          <p>Customize the watermark style and test it live</p>
        </div>
        <cr-button @click="${this.onCopyJsonClick_}">
            Copy style as JSON
        </cr-button>
      </div>

      <div class="control-row">
        <span>Font size</span>
        <div class="number-control-container">
          <cr-input
              id="fontSizeInput"
              class="font-size-input stroked"
              aria-label="Font size"
              type="number"
              min="${FONT_SIZE_MIN}"
              max="${FONT_SIZE_MAX}"
              @keydown="${this.onFontSizeInputKeyDown_}"
              @value-changed="${this.onFontSizeChanged_}">
          </cr-input>
          <div class="spinner-buttons">
            <button
              class="spinner-btn up"
              ?disabled="${this.fontSize_ >= FONT_SIZE_MAX}"
              @click="${this.onIncrementFontSize_}"
              @mousedown="${this.onFontSizeInputMouseDown_}">
            </button>
            <button
              class="spinner-btn down"
              ?disabled="${this.fontSize_ <= FONT_SIZE_MIN}"
              @click="${this.onDecrementFontSize_}"
              @mousedown="${this.onFontSizeInputMouseDown_}">
            </button>
          </div>
        </div>
      </div>

      <div id="fontSizeInputError">
        <span>
          Font size should be between ${FONT_SIZE_MIN} and ${FONT_SIZE_MAX}
        </span>
      </div>

      <div class="control-row">
        <span>White outline opacity</span>
        <div class="slider-container">
          <cr-slider id="outlineOpacitySlider" aria-label="White outline opacity"
              min="0" max="100"
              .value="${this.outlineOpacity_}"
              .ticks="${this.opacityTicks_}"
              @cr-slider-value-changed="${this.onOutlineOpacityChanged_}">
          </cr-slider>
          <span class="slider-percentage">${this.outlineOpacity_}%</span>
        </div>
      </div>

      <div class="control-row">
        <span>Dark fill opacity</span>
        <div class="slider-container">
          <cr-slider id="fillOpacitySlider" aria-label="Dark fill opacity"
              min="0" max="100"
              .value="${this.fillOpacity_}"
              .ticks="${this.opacityTicks_}"
              @cr-slider-value-changed="${this.onFillOpacityChanged_}">
          </cr-slider>
          <span class="slider-percentage">${this.fillOpacity_}%</span>
        </div>
      </div>
    </div>
  `;
}
