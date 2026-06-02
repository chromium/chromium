// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SplitViewData, TabData, TabGroupData} from './tab_data.js';
import {tokenToString} from './tab_data.js';
import type {TabSearchPageElement} from './tab_search_page.js';
import type {TitleItem} from './title_item.js';

// clang-format off
export function getHtml(this: TabSearchPageElement) {
  return html`<!--_html_template_start_-->
<div id="tabSearchPage">
  <div id="searchField" @keydown="${this.onSearchKeydown_}"
      clear-label="$i18n{clearSearch}">
    <cr-icon id="searchIcon" icon="tab-search:search"></cr-icon>
    <div id="searchWrapper">
      <label id="searchLabel" for="searchInput" aria-hidden="true">
        <span>$i18n{searchTabs}</span>
        <span>${this.shortcut_}</span>
        <span id="searchResultText">${this.searchResultText_}</span>
      </label>
      <input id="searchInput" aria-labelledby="searchLabel"
          autofocus autocomplete="off"
          maxlength="${this.searchQueryMaxLength_}"
          @search="${this.onSearchTermSearch}"
          @input="${this.onSearchTermInput}"
          type="search" spellcheck="false" role="combobox"
          aria-activedescendant="${this.activeSelectionId_ || nothing}"
          aria-controls="tabsList" aria-owns="tabsList">
    </div>
  </div>
  <div id="divider"></div>
  <div ?hidden="${!this.filteredItems_.length}">
    <selectable-lazy-list id="tabsList"
        max-height="${this.listMaxHeight_}"
        item-size="${this.listItemSize_}"
        .items="${this.filteredItems_}"
        @selected-change="${this.onSelectedChange_}"
        role="listbox"
        .isSelectable="${
            (item: TitleItem|TabData|TabGroupData|SplitViewData) => {
          return item.constructor.name === 'TabData' ||
              item.constructor.name === 'TabGroupData' ||
              item.constructor.name === 'SplitViewData';
        }}"
        .template="${
            (item: TitleItem|TabData|TabGroupData|SplitViewData,
             index: number) => {
      switch (item.constructor.name) {
       case 'TitleItem':
        this.assertIsTitleItem_(item);
        return html`
          <div class="list-section-title">
            <div>${item.title}</div>
            ${item.expandable ? html`<cr-expand-button
                  aria-label="$i18n{recentlyClosedExpandA11yLabel}"
                  data-title="${item.title}"
                  data-index="${index}"
                  expand-icon="cr:arrow-drop-down"
                  collapse-icon="cr:arrow-drop-up"
                  ?expanded="${item.expanded}"
                  expand-title="$i18n{expandRecentlyClosed}"
                  collapse-title="$i18n{collapseRecentlyClosed}"
                  @expanded-changed="${this.onTitleExpandedChanged_}"
                  no-hover>
              </cr-expand-button>` : ''}
          </div>`;
       case 'TabData':
        this.assertIsTabData_(item);
        return html`<tab-search-item id="${item.tab.tabId}"
            aria-label="${this.ariaLabel_(item)}"
            class="mwb-list-item selectable" .data="${item}"
            data-index="${index}"
            @click="${this.onItemClick_}"
            @close="${this.onItemClose_}"
            @focus="${this.onItemFocus_}"
            @keydown="${this.onItemKeydown_}"
            role="option"
            tabindex="0">
        </tab-search-item>`;
       case 'TabGroupData':
        this.assertIsTabGroupData_(item);
        return html`<tab-search-group-item
            id="${tokenToString(item.tabGroup.id)}"
            class="mwb-list-item selectable"
            .data="${item}"
            data-index="${index}"
            aria-label="${this.ariaLabel_(item)}"
            @click="${this.onItemClick_}"
            @focus="${this.onItemFocus_}"
            @keydown="${this.onItemKeydown_}"
            role="option" tabindex="0">
        </tab-search-group-item>`;
       case 'SplitViewData':
        this.assertIsSplitViewData_(item);
        return html`<tab-search-split-item
            id="${item.sessionId}"
            class="mwb-list-item selectable"
            .data="${item}"
            data-index="${index}"
            aria-label="${this.ariaLabel_(item)}"
            @click="${this.onItemClick_}"
            @focus="${this.onItemFocus_}"
            @keydown="${this.onItemKeydown_}"
            role="option" tabindex="0">
        </tab-search-split-item>`;
       default:
        return html``;
      }
    }}">
    </selectable-lazy-list>
  </div>
  <div id="no-results" ?hidden="${this.filteredItems_.length > 0}">
    $i18n{noResultsFound}
  </div>
</div>
<!--_html_template_end_-->`;
}
