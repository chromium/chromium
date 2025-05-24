// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {HorizontalCarouselElement} from './horizontal_carousel.js';

export function getHtml(this: HorizontalCarouselElement) {
  return html`
  <div id="backButtonContainer" class="carousel-button-container">
    <cr-icon-button id="backButton" class="carousel-button"
        @click="${this.onCarouselBackClick_}" iron-icon="cr:chevron-left"
        ?hidden="${!this.showBackButton_}" tabindex="-1">
    </cr-icon-button>
    <div id="backHoverLayer" class="hover-layer"></div>
  </div>

  <div id="carouselContainer">
    <div id="startProbe"></div>
    <slot name="table" id="slottedTable"></slot>
    <div id="endProbe"></div>
  </div>

  <div id="forwardButtonContainer" class="carousel-button-container">
    <cr-icon-button id="forwardButton" class="carousel-button"
        @click="${this.onCarouselForwardClick_}" iron-icon="cr:chevron-right"
        ?hidden="${!this.showForwardButton}" tabindex="-1">
    </cr-icon-button>
    <div id="forwardHoverLayer" class="hover-layer"></div>
  </div>`;
}
