// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SmartSearchElement} from './smart_search.js';

export function getHtml(this: SmartSearchElement) {
  return html`
    <main id="smart-search-view">
      <section>
        <h1>JumpStart</h1>

        <!-- Search Bar -->
        <div class="search-container">
          <div class="search-bar-wrapper">
            <cr-icon icon="cr:search"></cr-icon>
            <input
                class="search-input"
                type="text"
                placeholder="Describe what you're looking for"
                .value="${this.searchQuery_}"
                ?disabled="${!this.smartSearchEnabled_}"
                @input="${this.onSearchInput_}"
                @keydown="${this.onSearchInputKeydown_}" />
            <cr-button class="action-button search-button"
                ?disabled="${
  !this.smartSearchEnabled_ || !this.searchQuery_.trim()}"
                @click="${this.onSearchClick_}">
              Search
            </cr-button>
          </div>
        </div>
      </section>
    </main>
  `;
}
