// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TabPickerAppElement} from './tab_picker_app.js';

export function getHtml(this: TabPickerAppElement) {
  return html`<!--_html_template_start_-->
    <button id="shareTabsTrigger" class="dropdown-item"
        role="menuitem"
        aria-haspopup="menu"
        aria-expanded="${this.tabMenuOpen}"
        @pointerenter="${this.onShareTabsRowPointerenter_}"
        @pointerleave="${this.onShareTabsRowPointerleave_}"
        @click="${this.onShareTabsRowClick_}"
        @keydown="${this.onShareTabsRowKeydown_}">
      <cr-icon icon="composebox:tab"></cr-icon>
      <span class="tab-title">
        ${this.sharingTabsText_}
      </span>
      <composebox-favicon-group .tabs="${this.selectedTabs}">
      </composebox-favicon-group>
      <cr-icon class="share-tabs-arrow" icon="cr:chevron-right">
      </cr-icon>
    </button>

    <cr-action-menu id="tabMenu"
        role-description="menu"
        @close="${this.onMenuClose_}"
        @pointerenter="${this.onMenuPointerenter_}"
        @pointerleave="${this.onMenuPointerleave_}">
      ${this.tabSuggestions.map((tab, index) => html`
        <div class="suggestion-container" role="presentation">
          <button class="dropdown-item"
              role="menuitem"
              title="${tab.title}" data-index="${index}"
              aria-label="${tab.title}"
              @click="${this.onTabClick_}">
            <cr-composebox-tab-favicon .url="${tab.url}"
                .tabId="${tab.tabId}"
                .isSubmitted="${this.isTabSelected_(tab)}">
            </cr-composebox-tab-favicon>
            <span class="tab-title-group">
              <span class="tab-title">${tab.title}</span>
              ${this.isRecentTab_(tab.tabId) ? html`
                <span class="recent-tabs-suffix">
                  · ${this.getRecentTabsSuffix_()}
                </span>
              ` : ''}
            </span>
            ${this.isTabSelected_(tab) ? html`
              <cr-icon class="share-tabs-check" icon="cr:check">
              </cr-icon>
            ` : ''}
          </button>
        </div>
      `)}
    </cr-action-menu>
  <!--_html_template_end_-->`;
}
