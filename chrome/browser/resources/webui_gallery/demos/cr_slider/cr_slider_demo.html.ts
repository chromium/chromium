// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrSliderDemoElement} from './cr_slider_demo.js';

export function getHtml(this: CrSliderDemoElement) {
  return html`
<h1>cr-slider</h1>

<h2>Indiscrete</h2>
<div class="demos">
  <cr-slider id="basicSlider" min="0" max="20" .value="${this.basicValue_}"
      @cr-slider-value-changed="${this.onBasicValueChanged_}">
  </cr-slider>
  <div>Value of slider: ${this.basicValue_}</div>
</div>

<h2>5 ticks, increments of 5</h2>
<div class="demos">
  <cr-slider id="tickedSlider" .ticks="${this.ticks_}"
      marker-count="${this.getMarkerCount_()}"
      .value="${this.tickedValue_}"
      @cr-slider-value-changed="${this.onTickedValueChanged_}">
  </cr-slider>
  <div>Value of slider, the index of selected tick: ${this.tickedValue_}</div>
  <div>Value of selected tick: ${this.getTickValue_()}</div>
  <cr-checkbox ?checked="${this.showMarkers_}"
      @checked-changed="${this.onShowMarkersChanged_}">
    Show markers
  </cr-checkbox>
</div>

<h2>Disabled</h2>
<div class="demos">
  <cr-slider min="0" max="20" value="12" disabled></cr-slider>
</div>`;
}
