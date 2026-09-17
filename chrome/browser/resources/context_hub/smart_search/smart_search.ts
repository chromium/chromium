// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/icons.html.js';
import './smart_search_card.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';

import {getCss} from './smart_search.css.js';
import {getHtml} from './smart_search.html.js';
import type {SmartSearchResult} from './smart_search_card.js';

export type {SmartSearchResult} from './smart_search_card.js';

export class SmartSearchElement extends CrLitElement {
  static get is() {
    return 'smart-search';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      searchQuery_: {type: String},
      results_: {type: Array},
      selectedIds_: {type: Object},
      isSearching_: {type: Boolean},
      hasSearched_: {type: Boolean},
      smartSearchEnabled_: {type: Boolean},
    };
  }

  protected accessor searchQuery_: string = '';
  protected accessor results_: SmartSearchResult[] = [];
  protected accessor selectedIds_: Set<string> = new Set();
  protected accessor isSearching_: boolean = false;
  protected accessor hasSearched_: boolean = false;
  protected accessor smartSearchEnabled_: boolean =
      loadTimeData.getBoolean('kSmartSearch');

  protected getFilteredResults_(): SmartSearchResult[] {
    return this.results_;
  }

  protected onSearchInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.executeSearch_();
    }
  }

  protected onSearchInput_(e: Event) {
    const input = e.target as HTMLInputElement;
    this.searchQuery_ = input.value;
  }

  protected onSearchClick_() {
    this.executeSearch_();
  }

  private async executeSearch_() {
    if (!this.smartSearchEnabled_) {
      return;
    }

    const query = this.searchQuery_.trim();
    if (!query) {
      return;
    }

    this.isSearching_ = true;
    this.hasSearched_ = true;
    this.selectedIds_ = new Set();
    this.results_ = [];

    try {
      const {results} =
          await browserProxyFactory.getInstance().handler.executeSmartSearch(
              query);
      if (results) {
        const mappedResults: SmartSearchResult[] = [];
        results.forEach((item, index) => {
          const refs = item.sourceReferences || [];
          refs.forEach((ref, refIndex) => {
            if (!ref.drive) {
              return;
            }
            const title = ref.drive.name;
            const url = ref.drive.url;
            if (!title || !url) {
              return;
            }
            mappedResults.push({
              id: `${index}-${refIndex}`,
              title,
              url,
              snippet: item.description,
            });
          });
        });
        this.results_ = mappedResults;
      }
    } catch (e) {
      console.error('Smart search error:', e);
      this.results_ = [];
    } finally {
      this.isSearching_ = false;
    }
  }

  protected onCardSelectionChange_(
      e: CustomEvent<{id: string, selected: boolean}>) {
    const {id, selected} = e.detail;
    if (selected) {
      this.selectedIds_.add(id);
    } else {
      this.selectedIds_.delete(id);
    }
    this.selectedIds_ = new Set(this.selectedIds_);
  }

  protected onOpenSelectedClick_() {
    const selectedItems =
        this.results_.filter(r => this.selectedIds_.has(r.id));
    for (const item of selectedItems) {
      window.open(item.url, '_blank');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'smart-search': SmartSearchElement;
  }
}

customElements.define(SmartSearchElement.is, SmartSearchElement);
