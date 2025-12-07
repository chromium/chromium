// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ActivityLogHistoryItemElement} from './activity_log_history_item.js';

export function getHtml(this: ActivityLogHistoryItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div ?actionable="${this.isExpandable_}" id="activity-item-main-row"
    @click="${this.onExpandClick_}">
  <div id="activity-call-and-count">
    <span id="activity-type">${this.data.activityType}</span>
    <span id="activity-key" title="${this.data.key}">${this.data.key}</span>
    <span id="activity-count">${this.data.count}</span>
  </div>
  <cr-expand-button no-hover ?expanded="${this.expanded_}"
      ?hidden="${!this.isExpandable_}"
      @expanded-changed="${this.onExpandedChanged_}">
  </cr-expand-button>
  <div class="separator" ?hidden="${!this.isExpandable_}"></div>
  <cr-icon-button id="activity-delete" class="icon-delete-gray"
      aria-describedby="api-call" aria-label="$i18n{clearEntry}"
      @click="${this.onDeleteClick_}">
  </cr-icon-button>
</div>
<div id="page-url-list" ?hidden="${!this.expanded_}">
  ${this.getPageUrls_().map(item => html`
    <div class="page-url">
      <a class="page-url-link" href="${item.page}" target="_blank"
          title="${item.page}">
        ${item.page}
      </a>
      <span class="page-url-count" ?hidden="${!this.shouldShowPageUrlCount_()}">
        ${item.count}
      </span>
    </div>`)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
