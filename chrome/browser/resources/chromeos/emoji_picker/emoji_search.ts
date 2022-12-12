// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import './emoji_button.js';
import './emoji_category_button.js';
import './emoji_group.js';

import {CrSearchFieldElement} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import {PolymerSpliceChange} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './emoji_search.html.js';
import Fuse from './fuse.js';
import {CategoryData, CategoryEnum, EmojiGroupData, EmojiVariants} from './types.js';

export interface EmojiSearch {
  $: {
    search: CrSearchFieldElement,
    searchShadow: HTMLElement,
  };
}


export class EmojiSearch extends PolymerElement {
  static get is() {
    return 'emoji-search' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      categoriesData: {type: Array, readonly: true},
      categoryMetadata: {type: Array, readonly: true},
      lazyIndexing: {type: Boolean, value: true},
      searchResults: {type: Array},
      v2Enabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        readonly: true,
      },
      needIndexing: {type: Boolean, value: false},
    };
  }
  categoriesData: EmojiGroupData;
  categoryMetadata: CategoryData[];
  lazyIndexing: boolean;
  private searchResults: EmojiGroupData;
  private v2Enabled: boolean;
  private needIndexing: boolean;
  // TODO(b/235419647): Update the config to use extended search.
  private fuseConfig: Fuse.IFuseOptions<EmojiVariants> = {
    threshold: 0.0,        // Exact match only.
    ignoreLocation: true,  // Match in all locations.
    keys:
        [
          {name: 'base.name', weight: 10},  // Increase scoring of emoji name.
          'base.keywords',
        ],
  };
  private fuseInstances = new Map<CategoryEnum, Fuse<EmojiVariants>>();

  static get observers() {
    return [
      'categoriesDataChanged(categoriesData.splices,lazyIndexing)',
    ];
  }

  override ready() {
    super.ready();

    this.addEventListener('scroll', () => this.onSearchScroll());
    // Cast here is safe since that is the spec from the cr search field mixin.
    this.addEventListener(
        'search', (ev) => this.onSearch((ev as CustomEvent<string>).detail));
    this.$.search.getSearchInput().addEventListener(
        'keydown', (ev: KeyboardEvent) => this.onSearchKeyDown(ev));
    this.addEventListener('keydown', (ev: KeyboardEvent) => this.onKeyDown(ev));
  }

  private onSearch(newSearch: string): void {
    this.searchResults = this.computeSearchResults(newSearch);
  }

  /**
   * Processes the changes to the data and determines whether indexing is
   * needed or not. It also triggers indexing if mode is not lazy and there
   * are new changes.
   *
   */
  private categoriesDataChanged(
      changedRecords: PolymerSpliceChange<EmojiGroupData>,
      lazyIndexing: boolean): void {
    if (!changedRecords && lazyIndexing) {
      return;
    }

    // Indexing is needed if there are new changes.
    this.needIndexing = this.needIndexing ||
        changedRecords.indexSplices.some(
            (s) => s.removed.length + s.addedCount > 0);

    // Trigger indexing if mode is not lazy and indexing is needed.
    if (!lazyIndexing && this.needIndexing) {
      this.createSearchIndices();
    }
  }

  /**
   * Event handler for keydown anywhere in the search component.
   * Used to move the focused result up/down on arrow presses.
   */
  private onKeyDown(ev: KeyboardEvent): void {
    // TODO(b/233567886): Implement navigation by keyboard for V2.
    if (this.v2Enabled) {
      return;
    }

    const isUp = ev.key === 'ArrowUp';
    const isDown = ev.key === 'ArrowDown';
    const isEnter = ev.key === 'Enter';
    // get emoji-button which has focus.
    const focusedResult =
        this.shadowRoot!.querySelector<HTMLElement>('.result:focus-within');

    if (isEnter && focusedResult) {
      focusedResult.click();
    }
    if (!isUp && !isDown) {
      return;
    }

    ev.preventDefault();
    ev.stopPropagation();

    // TODO(v/b/234673356): Move the navigation logic to emoji-group.
    if (!focusedResult) {
      return;
    }

    const prev = focusedResult.previousElementSibling as HTMLElement | null;
    const next = focusedResult.nextElementSibling as HTMLElement | null;

    // moving up from first result focuses search box.
    // need to check classList in case prev is sr-only.
    if (isUp && prev && !prev.classList.contains('result')) {
      this.$.search.getSearchInput().focus();
      return;
    }

    const newResult = isDown ? next : prev;
    if (newResult) {
      newResult.focus();
    }
  }

  /**
   * Event handler for keydown on the search input. Used to switch focus to the
   * results list on down arrow or enter key presses.
   */
  private onSearchKeyDown(ev: KeyboardEvent): void {
    const resultsCount = this.getNumSearchResults();
    // if not searching or no results, do nothing.
    if (!this.$.search.getValue() || resultsCount === 0) {
      return;
    }

    const isDown = ev.key === 'ArrowDown';
    const isEnter = ev.key === 'Enter';
    const isTab = ev.key === 'Tab';
    if (isDown || isEnter || isTab) {
      ev.preventDefault();
      ev.stopPropagation();

      // TODO(b/234673356): Remove this block.
      if (!this.v2Enabled) {
        // focus first item in result list.
        const firstButton =
            this.shadowRoot!.querySelector<HTMLElement>('.result');
        if (firstButton) {
          firstButton.focus();

          // if there is only one result, select it on enter.
          if (isEnter && resultsCount === 1) {
            // ! is safe here since we are getting the first result
            firstButton.querySelector('emoji-button')!.click();
          }
        }
      } else {
        if (resultsCount === 0) {
          return;
        }

        const firstResultButton = this.findFirstResultButton();

        if (!firstResultButton) {
          throw new Error('Cannot find search result buttons.');
        }

        if (isEnter && resultsCount === 1) {
          firstResultButton.click();
        } else {
          firstResultButton.focus();
        }
      }
    }
  }

  /**
   * Format the emoji data for search:
   * 1) Remove duplicates.
   * 2) Remove groupings.
   */
  private preprocessDataForIndexing(emojiData: EmojiGroupData):
      EmojiVariants[] {
    // TODO(b/235419647): Remove addition of extra space.
    return Array.from(
        new Map(emojiData.map(group => group.emoji).flat(1).map(emoji => {
          // The Fuse search library in ChromeOS doesn't support prefix
          // matching. A workaround is appending a space before all name and
          // keyword labels. This allows us to force a prefix matching by
          // prepending a space on users' searches. E.g. for the Emoji "smile
          // face", we store " smile face", if the user searches for "fa", the
          // search will be " fa" and will match " smile face", but not "
          // infant".
          emoji.base.name = ' ' + emoji.base.name;
          if (emoji.base.keywords && emoji.base.keywords.length > 0) {
            emoji.base.keywords =
                emoji.base.keywords.map(keyword => ' ' + keyword);
          }
          return [emoji.base.string, emoji];
        })).values());
  }

  /**
   * Indexes category data for search.
   *
   * Note: The indexing is done for all data from scratch, it is possible
   * to index only the new changes with the cost of increasing logic
   * complexity.
   */
  private createSearchIndices(): void {
    if (!this.categoriesData || this.categoriesData.length === 0) {
      return;
    }

    // Get the list of unique categories in the order they appeared
    // in the data.
    const categories =
        [...new Set(this.categoriesData.map(item => item.category))];

    // Remove existing indices.
    this.fuseInstances.clear();

    for (const category of categories) {
      // Filter records for the category and preprocess them.
      const indexableEmojis =
          this.preprocessDataForIndexing(this.categoriesData.filter(
              emojiGroup => emojiGroup.category === category));

      // Create a new index for the category.
      this.fuseInstances.set(
          category, new Fuse(indexableEmojis, this.fuseConfig));
    }
    this.needIndexing = false;
  }

  private onSearchScroll(): void {
    if (!this.v2Enabled) {
      this.$.searchShadow.style.boxShadow =
          this.shadowRoot!.getElementById('results')?.scrollTop ?? 0 > 0 ?
          'var(--cr-elevation-3)' :
          'none';
    }
  }

  /**
   * Computes search results for a keyword.
   *
   */
  computeSearchResults(search: string|null): EmojiGroupData {
    if (!search) {
      return [];
    }

    // Index data if needed (for lazy mode).
    if (this.needIndexing) {
      this.createSearchIndices();
    }

    // TODO(b/235419647): Use `^${search}|'" ${search}"'` for extended search.
    // Add an initial space to force prefix matching only.
    const prefixSearchTerm = ` ${search}`;

    const searchResults: EmojiGroupData = [];

    // Search the keyword in the fuse instance of each category.
    for (const [category, fuseInstance] of this.fuseInstances.entries()) {
      const categorySearchResult =
          fuseInstance.search(prefixSearchTerm).map(item => item.item);

      // Add the category results if not empty.
      if (categorySearchResult.length !== 0) {
        searchResults.push({
          'category': category,
          'group': '',
          'emoji': categorySearchResult,
          'searchOnly': false,
        });
      }
    }

    return searchResults;
  }

  private onResultClick(ev: MouseEvent): void {
    // If the click is on elements except emoji-button, trigger the click on
    // the emoji-button.
    if ((ev.target as HTMLElement | null)?.nodeName !== 'EMOJI-BUTTON') {
      // Using ! here, since if we use ! we should at least get a crash (and
      // nothing would happen if we fall through via ?. )
      (ev.currentTarget as HTMLElement | null)!.querySelector('emoji-button')!
          .shadowRoot!.querySelector('button')!.click();
    }
  }

  /**
   * Finds the first button in the search result page.
   *
   */
  private findFirstResultButton(): HTMLElement|null {
    const results = this.shadowRoot!.querySelector('#search-results')
                        ?.querySelectorAll('emoji-group');
    if (results) {
      for (const result of results) {
        const button = result.firstEmojiButton();
        if (button) {
          return button;
        }
      }
    }
    return null;
  }

  /**
   * Determines visibility of the search results for V1.
   *
   */
  private shouldShowV1Results(
      searchResults: EmojiGroupData, v2Enabled: boolean): boolean {
    // TODO(b/234673356): Remove this function.
    return !v2Enabled && searchResults.length > 0;
  }

  /**
   * Calculates the total number of items in the search results.
   *
   */
  private getNumSearchResults(): number {
    return this.searchResults ?
        this.searchResults.reduce((acc, item) => acc + item.emoji.length, 0) :
        0;
  }

  /**
   * Checks if the search query is empty
   *
   */
  searchNotEmpty(): boolean {
    return this.$.search.getValue() !== '';
  }

  /**
   * Sets the search query
   */
  setSearchQuery(value: string): void {
    this.$.search.setValue(value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiSearch.is]: EmojiSearch;
  }
}

customElements.define(EmojiSearch.is, EmojiSearch);
