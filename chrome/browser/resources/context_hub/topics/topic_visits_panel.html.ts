// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicVisitsPanelElement} from './topic_visits_panel.js';

export function getHtml(this: TopicVisitsPanelElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="banner" role="note">
  <cr-icon icon="cr:info" aria-hidden="true"></cr-icon>
  <span>
    <strong>Fishfood Experiment Only:</strong> This tab is only visible during
    the fishfood evaluation to collect feedback on topic visits and will not
    appear in the actual product.
  </span>
</div>
<p id="instructions">
  Mark any visit that doesn't belong to this topic with ✕.
</p>
<ul id="visitList">
  ${this.visits_.map((visit, index) => html`
    <li class="visit" ?rejected="${this.isRejected_(visit)}">
      <span class="favicon" aria-hidden="true"
          style="background-image: ${this.getFavicon_(visit.url)}">
      </span>
      <div class="visit-text">
        <div class="visit-title">${visit.title || visit.domain}</div>
        <div class="visit-details">
          <span class="visit-url" title="${visit.url}">${visit.domain}</span>
          <span aria-hidden="true">·</span>
          <span class="visit-time">${visit.time}</span>
        </div>
      </div>
      ${this.isRejected_(visit) ? html`
        <span class="rejected-label" aria-hidden="true">Doesn't belong</span>
      ` : ''}
      <cr-icon-button class="reject" data-index="${index}"
          iron-icon="${this.isRejected_(visit) ?
              'cr:cancel-filled' : 'cr:close'}"
          title="${this.isRejected_(visit) ? 'Undo' : 'Doesn\'t belong'}"
          aria-label="${this.getRejectAriaLabel_(visit)}"
          aria-pressed="${this.isRejected_(visit)}"
          @click="${this.onRejectClick_}">
      </cr-icon-button>
    </li>
  `)}
</ul>
<!--_html_template_end_-->`;
  // clang-format on
}
