// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MemoryBanksElement} from './memory_banks.js';

export function getHtml(this: MemoryBanksElement) {
  return html`
    <div class="table-container">
      <table class="memories-table">
        <thead>
          <tr>
            <th class="col-checkbox">
              <cr-checkbox class="no-label"
                  ?checked="${this.isAllSelected_()}"
                  ?indeterminate="${this.isSomeSelected_()}"
                  @change="${this.onSelectAllChange_}">
              </cr-checkbox>
            </th>
            <th class="col-title">Title</th>
            <th class="col-collection">Collection</th>
            <th class="col-tags">Tags</th>
            <th class="col-note">Note</th>
            <th class="col-url">URL</th>
            <th class="col-date">Date</th>
            <th class="col-actions"></th>
          </tr>
        </thead>
        <tbody>
          ${
      this.getPaginatedEntries_().map(
          (item, index) => html`
            <tr class="${this.isSelected(item.id) ? 'selected' : ''}">
              <td class="col-checkbox">
                <cr-checkbox class="no-label"
                    data-id="${item.id}"
                    ?checked="${this.isSelected(item.id)}"
                    @change="${this.onCheckboxChange}"
                    @click="${this.onCheckboxClick}">
                </cr-checkbox>
              </td>
              <td class="col-title">
                <div class="title-cell-content">
                  <div class="table-favicon"
                      style="background-image: ${
              getFaviconForPageURL(item.url, true)}">
                  </div>
                  <a class="table-title-link"
                      href="${item.url}"
                      target="_blank"
                      title="${item.tabTitle}">
                    ${item.tabTitle}
                  </a>
                </div>
              </td>
              <td class="col-collection">
                ${
              item.collection ? html`
                  <span class="table-collection-badge">
                    ${item.collection}
                  </span>
                ` :
                                ''}
              </td>
              <td class="col-tags">
                ${
              item.tags && item.tags.length > 0 ? html`
                  <div class="table-tags-container">
                    ${item.tags.map(tag => html`
                      <span class="table-tag-badge">
                        ${tag}
                      </span>
                    `)}
                  </div>
                ` :
                                                  ''}
              </td>
              <td class="col-note">
                ${item.note || ''}
              </td>
              <td class="col-url">
                <a class="table-url-link"
                    href="${item.url}"
                    target="_blank"
                    title="${item.url}">
                  <span>
                    ${this.formatDisplayUrl_(item.url)}
                  </span>
                  <cr-icon icon="cr:open-in-new"></cr-icon>
                </a>
              </td>
              <td class="col-date">
                ${this.formatDate_(item.timestamp)}
              </td>
              <td class="col-actions">
                <cr-icon-button class="table-more-btn"
                    iron-icon="cr:more-vert"
                    title="More actions"
                    data-index="${index}"
                    @click="${this.onMoreActionsClick_}">
                </cr-icon-button>
              </td>
            </tr>
          `)}
        </tbody>
      </table>
      <div class="table-footer">
        <span>${this.getPaginationInfo_()}</span>
        <div class="table-pagination-controls">
          <cr-button class="pagination-btn"
              ?disabled="${this.currentPage_ === 0}"
              @click="${this.onPreviousPageClick_}">
            Previous
          </cr-button>
          <cr-button class="pagination-btn"
              ?disabled="${this.isLastPage_()}"
              @click="${this.onNextPageClick_}">
            Next
          </cr-button>
        </div>
      </div>
    </div>
  `;
}
