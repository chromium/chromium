// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DiscardsTabElement} from './discards_tab.js';

export function getHtml(this: DiscardsTabElement) {
  //clang-format off
  // TODO(crbug.com/399740817): Enable "Urgent discard a tab now" when
  // PageDiscardingHelper is enabled on Android.
  return html`<!--_html_template_start_-->
<div id="discards">
<if expr="not is_android">
  <div is="action-link" @click="${this.discardUrgentNow_}">
    [Urgent discard a tab now]
  </div>
</if>
  <div is="action-link" @click="${this.toggleBatterySaverMode_}">
    [Toggle battery saver mode]
  </div>
  ${this.isPerformanceInterventionDemoModeEnabled_ ? html`
    <div is="action-link"
        @click="${this.refreshPerformanceTabCpuMeasurements_}">
      [Trigger Performance CPU intervention]
    </div>
  ` : ''}
  <table id="tab-discard-info-table">
    <thead >
      <tr id="tab-discards-info-table-header">
        <th data-sort-key="utilityRank" class="sort-column"
            @click="${this.onSortClick}">
          <div class="header-cell-container">
            <div>
              <div>Utility</div>
              <div>Rank</div>
            </div>
          </div>
        </th>
        <th data-sort-key="siteEngagementScore"
            @click="${this.onSortClick}">
          <div class="header-cell-container">
            <div>
              <div>Site</div>
              <div>Engagement</div>
              <div>Score</div>
            </div>
          </div>
        </th>
        <th data-sort-key="title" @click="${this.onSortClick}">
          <div class="header-cell-container">
            Tab Title
          </div>
        </th>
        <th data-sort-key="tabUrl" @click="${this.onSortClick}">
          <div class="header-cell-container">
            Tab URL
          </div>
        </th>
        <th data-sort-key="visibility" @click="${this.onSortClick}">
          <div class="header-cell-container">
            Visibility
          </div>
        </th>
        <th data-sort-key="loadingState" @click="${this.onSortClick}">
          <div class="header-cell-container">
            Loading State
          </div>
        </th>
        <th data-sort-key="state" @click="${this.onSortClick}">
          <div class="header-cell-container">
            <div>
              <div>Lifecycle</div>
              <div>State</div>
            </div>
          </div>
        </th>
        <th data-sort-key="canDiscard" @click="${this.onSortClick}">
          <div class="header-cell-container">
            <div>
              <div>Is</div>
              <div>Discardable</div>
            </div>
          </div>
        </th>
        <th data-sort-key="canFreeze" @click="${this.onSortClick}">
          <div class="header-cell-container">
            <div>
              <div>Is</div>
              <div>Freezable</div>
            </div>
          </div>
        </th>
        <th data-sort-key="discardCount" @click="${this.onSortClick}">
          <div class="header-cell-container">
            <div>
              <div>Discard</div>
              <div>Count</div>
            </div>
          </div>
        </th>
        <th data-sort-key="isAutoDiscardable" @click="${this.onSortClick}">
          <div class="header-cell-container">
            <div>
              <div>Auto</div>
              <div>Discardable</div>
            </div>
          </div>
        </th>
        <th data-sort-key="lastActiveSeconds" @click="${this.onSortClick}">
          <div class="header-cell-container">
            Last Active
          </div>
        </th>
        <th>
          <div class="header-cell-container">
            Actions
          </div>
        </th>
      </tr>
    </thead>
    <tbody id="tab-discards-info-table-body">
      ${this.getSortedTabInfos_().map(item => html`
        <tr>
          <td>${item.utilityRank}</td>
          <td>${this.getSiteEngagementScore_(item)}</td>
          <td>
            <div class="title-cell-container">
              <div class="favicon-div"
                .style="${this.getFavIconStyle_(item)}"></div>
              <div class="title-cell">${item.title}</div>
            </div>
          </td>
          <td class="tab-url-cell">${item.tabUrl}</td>
          <td>${this.visibilityToString_(item.visibility)}</td>
          <td>${this.loadingStateToString_(item.loadingState)}</td>
          <td>${this.getLifeCycleState_(item)}</td>
          <td class="boolean-cell">
            <div>${this.boolToString_(item.canDiscard)}</div>
            <div is="action-link" class="tooltip-container"
              ?disabled="${!this.shouldShowCannotDiscardReason_(item)}">
              [View Reason]
              <div class="tooltip">${item.cannotDiscardReasons}<div>
            </div>
          </td>
          <td class="boolean-cell">
            <div>${this.canFreezeToString_(item.canFreeze)}</div>
            <div is="action-link" class="tooltip-container"
              ?disabled="${!this.shouldShowCannotFreezeReason_(item)}">
              [View Reason]
              <div class="tooltip">${item.cannotFreezeReasons}<div>
            </div>
          </td>
          <td>${item.discardCount}</td>
          <td class="boolean-cell">
            <div>${this.boolToString_(item.isAutoDiscardable)}</div>
            <div is="action-link" class="is-auto-discardable-link"
                data-id="${item.id}"
                data-is-auto-discardable="${item.isAutoDiscardable}"
                @click="${this.toggleAutoDiscardable_}">
              [Toggle]
            </div>
          </td>
          <td class="last-active-cell">
            ${this.durationToString_(item.lastActiveSeconds)}
          </td>
          <td class="actions-cell">
            <div is="action-link" data-id="${item.id}"
                @click="${this.loadTab_}"
                ?disabled="${!this.canLoadViaUi_(item)}">
                [Load]</div>
            <div is="action-link" data-id="${item.id}"
                @click="${this.urgentDiscardTab_}"
                ?disabled="${!this.canDiscardViaUi_(item)}">
              [Urgent Discard]
            </div>
            <div is="action-link" data-id="${item.id}"
                @click="${this.proactiveDiscardTab_}"
                ?disabled="${!this.canDiscardViaUi_(item)}">
              [Proactive Discard]
            </div>
            <div is="action-link" data-id="${item.id}"
                @click="${this.freezeTab_}"
                ?disabled="${!this.canFreezeViaUi_(item)}">
              [Freeze]
            </div>
          </td>
        </tr>
        `)}
    </tbody>
  </table>
</div>
<!--_html_template_end_-->`;
  //clang-format on
}
