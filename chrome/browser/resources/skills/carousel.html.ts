/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SkillsCarouselElement} from './carousel.js';

export function getHtml(this: SkillsCarouselElement) {
  return html`
<div id="carouselWrapper">
  <div id="carouselContainer">
    <cr-icon-button id="prevButton" iron-icon="cr:chevron-left"
        @click="${this.onPrevClick_}">
    </cr-icon-button>
    <cr-icon-button id="nextButton" iron-icon="cr:chevron-right"
        @click="${this.onNextClick_}">
    </cr-icon-button>
    <slot id="itemsSlot" name="items"></slot>
  </div>
</div>`;
}
