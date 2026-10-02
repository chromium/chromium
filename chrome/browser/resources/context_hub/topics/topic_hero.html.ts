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
    <!-- One badge with its emoji, centered on the origin. -->
    <g id="topic-hive-cell">
      <g transform="translate(-28, -28)">
        <path class="pattern-shape" d="${this.getBadgePath_()}"></path>
      </g>
      <text class="pattern-icon" x="0" y="2" font-size="22"
          text-anchor="middle" dominant-baseline="central">${
          this.getTextIcon_()}</text>
    </g>
    <pattern id="topic-hive-pattern" x="0" y="0" width="64" height="112"
        patternUnits="userSpaceOnUse">
      <use href="#topic-hive-cell" x="32" y="28"></use>
      <use href="#topic-hive-cell" x="0" y="84"></use>
      <use href="#topic-hive-cell" x="64" y="84"></use>
    </pattern>
  </defs>
  <rect width="100%" height="100%" fill="url(#topic-hive-pattern)"></rect>
</svg>
<div class="gradient-overlay"></div>
<!--_html_template_end_-->`;
  // clang-format on
}
