// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UnexportableKeysInternalsAppElement} from './app.js';

export function getHtml(this: UnexportableKeysInternalsAppElement) {
  return html`
<h1>Unexportable Keys Internals</h1>
<div id="deleteMatchingKeys">
  <input id="tagPrefixInput" type="text" aria-label="Key tag prefix"
      placeholder="Key tag prefix (empty matches keys without a tag)"
      .value="${this.tagPrefix_}" @input="${this.onTagPrefixInput_}">
  <cr-button id="deleteMatchingKeysButton"
      ?disabled="${!this.getMatchingKeys_().length}"
      @click="${this.onDeleteMatchingKeysClick_}">
    Delete matching keys (${this.getMatchingKeys_().length})
  </cr-button>
</div>
<table>
  <tr>
    ${this.columns_.map(col => html`
      <th class="sort-header" data-sort-key="${col.key}"
          @click="${this.onSortClick_}" @keydown="${this.onSortKeydown_}"
          tabindex="0" aria-sort="${this.getSortAttribute_(col.key)}">
        ${col.label}
      </th>
    `)}
    <th>Actions</th>
  </tr>
  ${this.unexportableKeysInfo_.map((item, index) => html`
    <tr>
      <td class="wrapped-key">${item.wrappedKey}</td>
      <td>${item.algorithm}</td>
      <td>${item.keyTag}</td>
      <td>${item.creationTime.toLocaleString()}</td>
      <td>
        <cr-icon-button class="icon-delete" iron-icon="cr:delete" title="Delete"
            data-index="${index}" @click="${this.onDeleteKeyClick_}">
        </cr-icon-button>
      </td>
    </tr>
  `)}
</table>
<cr-toast id="deleteErrorToast" duration="5000">
  <div>Error when deleting a key, please try again</div>
</cr-toast>`;
}
