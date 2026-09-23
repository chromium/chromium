// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicDetailsElement} from './topic_details.js';

export function getHtml(this: TopicDetailsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="details-wrapper" @scroll="${this.onScroll_}">
  <!-- 1. Background Image (Layer 1: moves behind title on scroll) -->
  <div class="hero-banner" style="--banner-bg: ${this.getBackgroundColor_()};">
    <svg class="hero-pattern-svg" width="100%" height="100%"
        xmlns="http://www.w3.org/2000/svg">
      <defs>
        <pattern id="topic-grid-pattern" x="0" y="0" width="64" height="64"
            patternUnits="userSpaceOnUse">
          <g transform="translate(4, 4)">
            <path class="hero-pattern-shape"
                d="${this.getBadgePath_()}"></path>
          </g>
          <text class="hero-pattern-icon" x="32" y="34" font-size="22"
              text-anchor="middle" dominant-baseline="central">${
              this.getTextIcon_()}</text>
        </pattern>
      </defs>
      <rect width="100%" height="100%" fill="url(#topic-grid-pattern)" />
    </svg>
    <div class="hero-gradient-overlay"></div>
  </div>

  <!-- 2. Sticky Header (Layer 3: sticks at top with fixed opaqueness) -->
  <div class="sticky-header ${this.isScrolled_ ? 'scrolled' : ''}">
    <div class="title-row">
      <h1 class="topic-title">${this.topic?.title}</h1>
      <!-- TODO(crbug.com/558572977): Use internationalized strings once GRD -->
      <!-- strings are added. -->
      <cr-button class="tonal-button"
          ?hidden="${!this.hasRelatedUrls_()}"
          @click="${this.onOpenRelatedTabsClick_}">
        Open related tabs
      </cr-button>
    </div>
    <div class="tabs-bar" role="tablist">
      <button class="tab-item active" role="tab" aria-selected="true"
          id="summary-tab" aria-controls="summary-tabpanel">
        Summary
        <span class="tab-indicator"></span>
      </button>
    </div>
  </div>

  <!-- 3. Details Page (Layer 2: scrolls above bg and under sticky header) -->
  <div class="content-area" id="summary-tabpanel" role="tabpanel"
      aria-labelledby="summary-tab">
    <p class="long-description">${this.getLongDescription_()}</p>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
