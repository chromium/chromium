// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CriticalActionsAppElement} from './app.js';

export function getHtml(this: CriticalActionsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<header class="flex-center">
  <h1 class="flex-center">
    Critical Actions Internals
    <span class="table-badge">SQL: CriticalActions</span>
  </h1>
  <div class="actions-group flex-center">
    <cr-button id="refresh-btn" @click="${this.onRefreshClick_}">
      Refresh
    </cr-button>
    <cr-button id="clear-all-btn" class="danger-btn"
        @click="${this.onClearAllClick_}">
      Clear Table
    </cr-button>
  </div>
</header>

${!this.isFeatureEnabled_ ? html`
  <div class="warning-banner flex-center">
    <span class="warning-icon">&#9888;</span>
    <div class="warning-content">
      <div class="warning-title">Critical Action History is disabled</div>
      <div class="warning-desc">
        Actions are not being recorded.&nbsp;
        Enable the feature flag to start logging.
      </div>
    </div>
    <a class="flags-link" href="chrome://flags/#critical-action-history"
        target="_blank">
      Open in chrome://flags &rsaquo;
    </a>
  </div>` : ''}

<div class="toolbar flex-center">
  <div class="filters-group flex-center">
    <div class="search-box flex-center">
      <input type="search" class="search-input"
          placeholder="Search ID, URL, Conversation, Task, Metadata..."
          .value="${this.searchQuery_}"
          @input="${this.onSearchInput_}">
    </div>

    <select
        .value="${this.actionTypeFilter_ === null ? '-1' : this.actionTypeFilter_.toString()}"
        @change="${this.onActionTypeChange_}">
      <option value="-1">All Action Types</option>
      <option value="1">FormFill</option>
      <option value="2">Download</option>
      <option value="3">SettingChange</option>
      <option value="4">CredentialAccess</option>
      <option value="5">GooglePasswordManager</option>
      <option value="6">FederatedLogin</option>
      <option value="7">CredentialsOtp</option>
      <option value="0">Unknown</option>
    </select>

    <select .value="${this.pageSize_.toString()}"
        @change="${this.onPageSizeChange_}">
      <option value="10">10 per page</option>
      <option value="25">25 per page</option>
      <option value="50">50 per page</option>
      <option value="100">100 per page</option>
    </select>
  </div>
</div>

<div class="table-container">
  <table>
    <thead>
      <tr>
        <th>Action ID</th>
        <th>Timestamp</th>
        <th>Visit ID</th>
        <th>Conversation ID</th>
        <th>Actor Task ID</th>
        <th>Type</th>
        <th>Source</th>
        <th>URL</th>
        <th>Metadata</th>
        <th>Actions</th>
      </tr>
    </thead>
    <tbody>
      ${this.entries_.map(entry => html`
        <critical-action-row
            .item="${entry}"
            @delete-entry="${this.onDeleteEntry_}">
        </critical-action-row>`)}
    </tbody>
  </table>

  ${this.entries_.length === 0 ? html`
    <div class="empty-state">
      No critical action records found in the database.
    </div>` : ''}
</div>

<div class="pagination-bar flex-center">
  <div class="pagination-info">
    ${this.getPaginationInfo_()}
  </div>

  <div class="pagination-controls flex-center">
    <button class="page-btn" title="First Page"
        ?disabled="${this.isFirstPage_()}"
        @click="${this.onFirstPageClick_}">
      &laquo;
    </button>
    <button class="page-btn" title="Previous Page"
        ?disabled="${this.isFirstPage_()}"
        @click="${this.onPrevPageClick_}">
      &lsaquo; Prev
    </button>

    <span class="page-current">
      ${this.pageIndex_ + 1} / ${this.computeTotalPages_()}
    </span>

    <button class="page-btn" title="Next Page"
        ?disabled="${this.isLastPage_()}"
        @click="${this.onNextPageClick_}">
      Next &rsaquo;
    </button>
    <button class="page-btn" title="Last Page"
        ?disabled="${this.isLastPage_()}"
        @click="${this.onLastPageClick_}">
      &raquo;
    </button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
