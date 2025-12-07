// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ActivityLogHistoryElement} from './activity_log_history.js';

export function getHtml(this: ActivityLogHistoryElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="activity-subpage-header">
  <cr-search-field label="$i18n{activityLogSearchLabel}"
      @search-changed="${this.onSearchChanged_}">
  </cr-search-field>
  <cr-button class="clear-activities-button"
      @click="${this.onClearActivitiesClick_}">
    $i18n{clearActivities}
  </cr-button>
  <cr-icon-button id="more-actions" iron-icon="cr:more-vert"
      title="$i18n{activityLogMoreActionsLabel}"
      @click="${this.onMoreActionsClick_}">
  </cr-icon-button>
  <cr-action-menu role-description="$i18n{menu}">
    <button id="expand-all-button" class="dropdown-item"
        @click="${this.onExpandAllClick_}">
      $i18n{activityLogExpandAll}
    </button>
    <button id="collapse-all-button" class="dropdown-item"
        @click="${this.onCollapseAllClick_}">
      $i18n{activityLogCollapseAll}
    </button>
    <button id="export-button" class="dropdown-item"
        @click="${this.onExportClick_}">
      $i18n{activityLogExportHistory}
    </button>
  </cr-action-menu>
</div>
<div id="loading-activities" class="activity-message"
    ?hidden="${!this.shouldShowLoadingMessage_()}">
  <span>$i18n{loadingActivities}</span>
</div>
<div id="no-activities" class="activity-message"
    ?hidden="${!this.shouldShowEmptyActivityLogMessage_()}">
  <span>$i18n{noActivities}</span>
</div>
<div class="activity-table-headings" ?hidden="${!this.shouldShowActivities_()}">
  <span id="activity-type">$i18n{activityLogTypeColumn}</span>
  <span id="activity-key">$i18n{activityLogNameColumn}</span>
  <span id="activity-count">$i18n{activityLogCountColumn}</span>
</div>
<div id="activity-list" ?hidden="${!this.shouldShowActivities_()}">
  ${this.activityData_.map(item => html`
    <activity-log-history-item .data="${item}">
    </activity-log-history-item>`)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
