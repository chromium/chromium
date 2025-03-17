// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ScrollViewDemoElement} from './scroll_view_demo.js';

export function getHtml(this: ScrollViewDemoElement) {
  // clang-format off
  return html`
<h1>Scroll view with shadows indicating scroll and dynamic height</h1>
<div class="demos">
  <div id="sliderContainer">
    <label id="sliderLabel">Number of items in scrollable container</label>
    <cr-slider id="itemsLengthSlider" min="0" max="30"
        .value="${this.items_.length}"
        aria-labelledby="sliderLabel"
        @cr-slider-value-changed="${this.onItemsLengthChanged_}">
    </cr-slider>
    ${this.items_.length}
  </div>

  <div id="layout">
    <div id="container" show-bottom-shadow>
      ${this.items_.map(
           _item => html`<div class="item" tabindex="0">Focusable item</div>`)}
    </div>
    <div class="can-scroll-log">can scroll</div>
    <div class="scrolled-to-top-log">scrolled to top</div>
    <div class="scrolled-to-bottom-log">scrolled to bottom</div>
  </div>
</div>

<h1>cr-scrollable</h1>
<div id="cr-scrollable-demos" class="demos">
  <div class="cr-scrollable">
    <div class="label">A normal scrollable element with no indicators.</div>
    <div class="block"></div>
  </div>
  <div class="cr-scrollable">
    <div class="cr-scrollable-top"></div>
    <div class="label">With borders indicating element is scrollable.</div>
    <div class="block"></div>
    <div class="cr-scrollable-bottom"></div>
  </div>
</div>`;
  // clang-format on
}
