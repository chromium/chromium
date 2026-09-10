// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicCardElement} from './topic_card.js';

export function getHtml(this: TopicCardElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.hasContent_() ? html`
    <!-- Left: Decorative Topic Badge -->
    <div class="topic-badge">
      ${this.getBadgeShape_() === 'flower' ? html`
        <svg viewBox="0 0 56 56" class="badge-svg-bg" aria-hidden="true">
          <path fill="${this.getBackgroundColor_()}" d="${this.getFlowerPath_()}"></path>
        </svg>
      ` : this.getBadgeShape_() === 'diamond' ? html`
        <svg viewBox="0 0 56 56" class="badge-svg-bg" aria-hidden="true">
          <rect x="9" y="9" width="38" height="38" rx="12"
              fill="${this.getBackgroundColor_()}" transform="rotate(45 28 28)"></rect>
        </svg>
      ` : this.getBadgeShape_() === 'circle' ? html`
        <svg viewBox="0 0 56 56" class="badge-svg-bg" aria-hidden="true">
          <circle cx="28" cy="28" r="26" fill="${this.getBackgroundColor_()}"></circle>
        </svg>
      ` : html`
        <svg viewBox="0 0 56 56" class="badge-svg-bg" aria-hidden="true">
          <path fill="${this.getBackgroundColor_()}" d="${this.getCloudPath_()}"></path>
        </svg>
      `}
      <span class="badge-icon" aria-hidden="true">
        ${this.isCrIcon_() ? html`
          <cr-icon .icon="${this.getIcon_()}"></cr-icon>
        ` : this.getIcon_()}
      </span>
    </div>

    <!-- Center: Topic Information -->
    <div class="topic-info">
      ${this.topic?.title?.trim() ? html`
        <h2 class="topic-title">${this.topic.title}</h2>
      ` : ''}
      ${this.topic?.description?.trim() ? html`
        <p class="topic-description">${this.topic.description}</p>
      ` : ''}
    </div>

    <!-- Right: Action Pill Button -->
    <cr-button class="tonal-button action-pill-button"
        aria-label="${this.getActionAriaLabel_()}"
        @click="${this.onJumpBackInClick_}">
      Jump back in
    </cr-button>
` : ''}
  <!--_html_template_end_-->`;
  // clang-format on
}
