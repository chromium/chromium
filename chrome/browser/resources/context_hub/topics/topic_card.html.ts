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
      <svg viewBox="0 0 56 56" class="badge-svg-bg" aria-hidden="true">
        <path fill="${this.getBackgroundColor_()}" d="${this.getBadgePath_()}">
        </path>
      </svg>
      <span class="badge-icon" aria-hidden="true">
        ${this.isCrIcon_() ? html`
          <cr-icon .icon="${this.getIcon_()}"></cr-icon>
        ` : html`${this.getIcon_()}`}
      </span>
    </div>

    <!-- Center: Topic Information -->
    <div class="topic-info">
      ${this.topic?.title.trim() ? html`
        <h2 class="topic-title">${this.topic.title}</h2>
      ` : ''}
      ${this.topic?.description.trim() ? html`
        <p class="topic-description">${this.topic.description}</p>
      ` : ''}
    </div>

    <!-- Right: Action Pill Button -->
    <cr-button class="tonal-button action-pill-button"
        aria-label="${this.getActionAriaLabel_()}"
        @click="${this.onJumpBackInClick_}">
      Jump back in
    </cr-button>

    ${this.feedbackEnabled_ ? html`
      <!-- Bottom: Temporary Fishfood Feedback, on its own row -->
      <topic-feedback-controls .topic="${this.topic}"
          .feedback="${this.feedback}">
      </topic-feedback-controls>
    ` : ''}
` : ''}
  <!--_html_template_end_-->`;
  // clang-format on
}
