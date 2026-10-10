// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicSummaryPanelElement} from './topic_summary_panel.js';

export function getHtml(this: TopicSummaryPanelElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<p id="longDescription">${this.topic?.longDescription || ''}</p>
<!-- TODO(crbug.com/558572977): Use internationalized strings once GRD -->
<!-- strings are added. -->
<cr-button id="openRelatedTabs" class="tonal-button"
    ?hidden="${!this.hasOpenableUrls_()}"
    @click="${this.onOpenRelatedTabsClick_}">
  Open related tabs
</cr-button>
${this.topic?.collections.map((collection, index) => html`
  <topic-collection-carousel .collection="${collection}"
      .feedback="${this.getCollectionFeedback_(index)}" data-index="${index}"
      @collection-feedback-change="${this.onCollectionFeedbackChange_}">
  </topic-collection-carousel>
`)}
${this.feedbackEnabled_ && this.queries_.length > 0 ? html`
  <!-- Temporary Fishfood Feedback. The queries are plain text, so that
       rating them doesn't open Glic. -->
  <section id="queryRatings" aria-labelledby="queryRatingsTitle"
      aria-describedby="queryRatingsHint">
    <p id="queryRatingsHint">
      Try these suggestions in Gemini in Chrome to see how useful they are.
    </p>
    <h2 id="queryRatingsTitle">Is this suggestion useful for this topic?</h2>
    <ul>
      ${this.queries_.map(query => html`
        <li class="query">
          <span class="query-text">${query.text}</span>
          <cr-icon-button class="query-like" data-index="${query.index}"
              data-liked="true"
              iron-icon="${this.isQueryRated_(query.index, true) ?
                  'cr:thumb-up-filled' : 'cr:thumb-up'}"
              title="Useful"
              aria-label="${this.getQueryAriaLabel_('Useful', query)}"
              aria-pressed="${this.isQueryRated_(query.index, true)}"
              @click="${this.onQueryRatingClick_}">
          </cr-icon-button>
          <cr-icon-button class="query-dislike" data-index="${query.index}"
              data-liked="false"
              iron-icon="${this.isQueryRated_(query.index, false) ?
                  'cr:thumb-down-filled' : 'cr:thumb-down'}"
              title="Not useful"
              aria-label="${this.getQueryAriaLabel_('Not useful', query)}"
              aria-pressed="${this.isQueryRated_(query.index, false)}"
              @click="${this.onQueryRatingClick_}">
          </cr-icon-button>
        </li>
      `)}
    </ul>
  </section>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
