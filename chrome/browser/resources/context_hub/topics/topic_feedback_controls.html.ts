// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicFeedbackControlsElement} from './topic_feedback_controls.js';

export function getHtml(this: TopicFeedbackControlsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="ratingRow">
  <cr-icon-button id="thumbsUp"
      iron-icon="${this.isLiked_() ? 'cr:thumb-up-filled' : 'cr:thumb-up'}"
      title="Good topic" aria-label="${this.getThumbsUpAriaLabel_()}"
      aria-pressed="${this.isLiked_()}"
      @click="${this.onThumbsUpClick_}">
  </cr-icon-button>
  <cr-icon-button id="thumbsDown"
      iron-icon="${this.isDisliked_() ? 'cr:thumb-down-filled' :
                                        'cr:thumb-down'}"
      title="Bad topic" aria-label="${this.getThumbsDownAriaLabel_()}"
      aria-pressed="${this.isDisliked_()}"
      @click="${this.onThumbsDownClick_}">
  </cr-icon-button>
</div>

${this.isDisliked_() ? html`
  <div id="defectsPanel">
    <div id="defectsLabel">What's wrong?</div>
    <div id="defectChips" role="group" aria-labelledby="defectsLabel">
      ${this.defectCategories_.map(item => html`
        <cr-chip data-category="${item.category}" title="${item.description}"
            .chipAriaLabel="${item.description}"
            ?selected="${this.isDefectSelected_(item.category)}"
            @click="${this.onDefectClick_}">
          <cr-icon icon="cr:check"
              ?hidden="${!this.isDefectSelected_(item.category)}">
          </cr-icon>
          ${item.label}
        </cr-chip>
      `)}
    </div>
    ${this.isDefectMissing_() ? html`
      <div id="defectError" role="alert">Select at least one reason</div>
    ` : ''}
    ${this.getSuggestionFields_().map(field => html`
      <cr-input class="suggestion" data-defect="${field.defect}"
          label="${field.label}" .value="${this.getSuggestion_(field.defect)}"
          @value-changed="${this.onSuggestionValueChanged_}"
          @change="${this.onCommentChange_}">
      </cr-input>
    `)}
    <cr-textarea id="comment" label="Comment" autogrow rows="2"
        .value="${this.getComment_()}"
        ?required="${this.isCommentRequired_()}"
        ?invalid="${this.isCommentMissing_()}"
        .firstFooter="${this.getCommentError_()}"
        @value-changed="${this.onCommentValueChanged_}"
        @change="${this.onCommentChange_}">
    </cr-textarea>
  </div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
