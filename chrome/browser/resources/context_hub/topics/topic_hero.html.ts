// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicHeroElement} from './topic_hero.js';

export function getHtml(this: TopicHeroElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<svg class="pattern" width="100%" height="100%"
    xmlns="http://www.w3.org/2000/svg">
  <defs>
    <pattern id="topic-grid-pattern" x="0" y="0" width="64" height="64"
        patternUnits="userSpaceOnUse">
      <!-- Centers the 56x56 badge in the 64x64 tile. -->
      <g transform="translate(4, 4)">
        <path class="pattern-shape" d="${this.getBadgePath_()}"></path>
      </g>
      <text class="pattern-icon" x="32" y="34" font-size="22"
          text-anchor="middle" dominant-baseline="central">${
          this.getTextIcon_()}</text>
    </pattern>
  </defs>
  <rect width="100%" height="100%" fill="url(#topic-grid-pattern)"></rect>
</svg>
<div class="gradient-overlay"></div>
<!--_html_template_end_-->`;
  // clang-format on
}
