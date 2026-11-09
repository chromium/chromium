// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TabPickerAppElement} from './tab_picker_app.js';

export function getHtml(this: TabPickerAppElement) {
  return html`<!--_html_template_start_-->
    <button id="shareTabsTrigger" class="dropdown-item"
        role="${this.isStsActiveRow_() ? 'menuitemcheckbox' : 'menuitem'}"
        aria-checked="${this.isStsActiveRow_() ? 'true' : nothing}"
        aria-haspopup="${this.isStsActiveRow_() ? nothing : 'menu'}"
        aria-expanded="${this.isStsActiveRow_() ? nothing : this.tabMenuOpen}"
        @pointerenter="${this.onShareTabsRowPointerenter_}"
        @pointermove="${this.onShareTabsRowPointermove_}"
        @pointerdown="${this.onShareTabsRowPointerdown_}"
        @pointerleave="${this.onShareTabsRowPointerleave_}"
        @focus="${this.onShareTabsRowFocus_}"
        @click="${this.onShareTabsRowClick_}"
        @keydown="${this.onShareTabsRowKeydown_}">
      <cr-icon icon="${this.isStsActiveRow_() ?
          'composebox:screensaverAuto' : 'composebox:tab'}"></cr-icon>
      <span class="tab-title">
        ${this.isStsActiveRow_() ?
            this.getShareOpenTabsText_() : this.sharingTabsText_}
      </span>
      ${this.isStsActiveRow_() ? html`
        <cr-icon id="smartTabSharingActiveCheck"
            class="share-tabs-check" icon="cr:check">
        </cr-icon>
      ` : html`
        <composebox-favicon-group .tabs="${this.selectedTabs}">
        </composebox-favicon-group>
        <cr-icon class="share-tabs-arrow" icon="cr:chevron-right">
        </cr-icon>
      `}
    </button>

    <cr-action-menu id="tabMenu"
        role-description="menu"
        non-modal
        @close="${this.onMenuClose_}"
        @unbounded-shown="${this.onMenuUnboundedShown_}"
        @unbounded-show-error="${this.onMenuUnboundedShowError_}"
        @pointerenter="${this.onMenuPointerenter_}"
        @pointermove="${this.onMenuPointermove_}"
        @pointerleave="${this.onMenuPointerleave_}"
        @keydown="${this.onMenuKeydown_}">
      ${this.smartTabSharingVisible ? html`
        <h4 id="automaticallyAddTabsHeader" aria-hidden="true">
          ${this.getAutomaticallyAddTabsHeader_()}
        </h4>
        <button id="smartTabSharingItemFlyout" class="dropdown-item"
            role="menuitemcheckbox"
            aria-checked="${this.smartTabSharingActive}"
            @click="${this.onSmartTabSharingItemClick_}">
          <cr-icon icon="composebox:screensaverAuto"></cr-icon>
          <span class="tab-title">
            ${this.getShareOpenTabsText_()}
          </span>
          ${this.smartTabSharingActive ? html`
            <cr-icon class="share-tabs-check" icon="cr:check">
            </cr-icon>
          ` : ''}
        </button>
        ${this.tabSuggestions.length > 0 ? html`
          <hr aria-hidden="true"/>
          <h4 id="manuallyAddTabsHeader" aria-hidden="true">
            ${this.getManuallyAddTabsHeader_()}
          </h4>
        ` : ''}
      ` : ''}
      ${this.tabSuggestions.map((tab, index) => html`
        <div class="suggestion-container" role="presentation">
          <button class="dropdown-item"
              ?disabled="${this.isTabDisabled_(tab)}"
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
