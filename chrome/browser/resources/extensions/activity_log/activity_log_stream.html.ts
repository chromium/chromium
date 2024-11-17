// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ActivityLogStreamElement} from './activity_log_stream.js';
import type {StreamItem} from './activity_log_stream_item.js';

export function getHtml(this: ActivityLogStreamElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="activity-subpage-header">
  <cr-search-field label="$i18n{activityLogSearchLabel}"
      @search-changed="${this.onSearchChanged_}">
  </cr-search-field>
  <cr-button id="toggle-stream-button" @click="${this.onToggleButtonClick_}">
    <span">
      ${this.isStreamOn_ ?
          html`$i18n{stopActivityStream}` : html`$i18n{startActivityStream}`}
    </span>
  </cr-button>
  <cr-button class="clear-activities-button" @click="${this.clearStream}">
    $i18n{clearActivities}
  </cr-button>
</div>
<div id="empty-stream-message" class="activity-message"
    ?hidden="${!this.isStreamEmpty_()}">
  <span id="stream-stopped-message" ?hidden="${this.isStreamOn_}">
    $i18n{emptyStreamStopped}
  </span>
  <span id="stream-started-message" ?hidden="${!this.isStreamOn_}">
    $i18n{emptyStreamStarted}
  </span>
</div>
<div id="empty-search-message" class="activity-message"
    ?hidden="${!this.shouldShowEmptySearchMessage_()}">
  $i18n{noSearchResults}
</div>
<div class="activity-table-headings"
    ?hidden="${this.isFilteredStreamEmpty_()}">
  <span id="activity-type">$i18n{activityLogTypeColumn}</span>
  <span id="activity-key">$i18n{activityLogNameColumn}</span>
  <span id="activity-time">$i18n{activityLogTimeColumn}</span>
</div>
<cr-infinite-list .items="${this.filteredActivityStream_}" item-size="56"
    .template="${(item: StreamItem) => html`
        <activity-log-stream-item .data="${item}">
        </activity-log-stream-item>`}">
</cr-infinite-list>
<!--_html_template_end_-->`;
  // clang-format on
}
