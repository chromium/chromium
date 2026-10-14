// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LogsAppElement} from './logs_app.js';
import {renderDropdown} from './logs_app_helpers.js';

export function getHtml(this: LogsAppElement) {
  // clang-format off
  return html`
<h1>Omnibox Debug Logs</h1>
<div>
  <div>Log Messages:</div>
  <table>
    <thead>
      <tr>
        <th class="time header-cell">Time</th>
        <th class="tag header-cell">
          <div class="header-container">
            <span>Tag</span>
            <button data-filter="tag"
                    ?disabled="${this.uniqueTags_.size <= 1}"
                    @click="${this.onDropdownToggleClick_}">▼</button>
            ${this.openFilterDropdown_ === 'tag' ?
                renderDropdown(
                    'tag', this.uniqueTags_, this.selectedTags_,
                    this.onFilterChange_) :
                ''}
          </div>
        </th>
        <th class="source-location header-cell">
          <div class="header-container">
            <span>Source Location</span>
            <button data-filter="source"
                    ?disabled="${this.uniqueSources_.size <= 1}"
                    @click="${this.onDropdownToggleClick_}">▼</button>
            ${this.openFilterDropdown_ === 'source' ?
                renderDropdown(
                    'source', this.uniqueSources_, this.selectedSources_,
                    this.onFilterChange_) :
                ''}
          </div>
        </th>
        <th class="message header-cell">Log Message</th>
        <th class="proto header-cell">
          <div class="header-container">
            <span>Proto</span>
            <button data-filter="proto"
                    ?disabled="${this.uniqueProtos_.size <= 1}"
                    @click="${this.onDropdownToggleClick_}">▼</button>
            ${this.openFilterDropdown_ === 'proto' ?
                renderDropdown(
                    'proto', this.uniqueProtos_, this.selectedProtos_,
                    this.onFilterChange_) :
                ''}
          </div>
        </th>
      </tr>
    </thead>
    <tbody>
      ${this.filteredMessages_.map(item => html`
        <tr>
          <td class="time">${item.eventTime.toLocaleTimeString()}</td>
          <td class="tag">${item.tag}</td>
          <td class="source-location">
            <a target="_blank" href="${item.sourceLinkUrl}">
              ${item.sourceLinkText}
            </a>
          </td>
          <td class="message">${item.message}</td>
          <td class="proto">
            ${item.protoType ? html`
              <a target="_blank"
                 href="${`http://protoshop/embed?tabs=viewer,editor,textproto&type=${item.protoType}&protobytes=${encodeURIComponent(item.protoBase64 || '')}`}">
                View ${item.protoType.split('.').pop()}
              </a>
            ` : ''}
          </td>
        </tr>
      `)}
    </tbody>
  </table>
</div>
`;
  // clang-format on
}
