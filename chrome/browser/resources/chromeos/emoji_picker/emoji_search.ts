// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import './emoji_category_button.js';
import './emoji_group.js';

import {CrSearchFieldElement} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import {PolymerSpliceChange} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NO_INTERNET_SEARCH_ERROR_MSG} from './constants.js';
import {Status} from './emoji_picker.mojom-webui.js';
import {EmojiPickerApiProxyImpl} from './emoji_picker_api_proxy.js';
import {getTemplate} from './emoji_search.html.js';
import {GIF_ERROR_TRY_AGAIN} from './events.js';
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
      needIndexing: {type: Boolean, value: false},
      gifSupport: {type: Boolean, value: false},
      status: {type: Status, value: null},
      searchQuery: {type: String, value: ''},
      nextGifPos: {type: String, value: ''},
      errorMessage: {type: String, value: NO_INTERNET_SEARCH_ERROR_MSG},
    };
  }
  categoriesData: EmojiGroupData;
  categoryMetadata: CategoryData[];
  lazyIndexing: boolean;
  private searchResults: EmojiGroupData;
  private needIndexing: boolean;
  private gifSupport: boolean;
  private status: Status|null;
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
  private nextGifPos: string;  // This variable ensures that we get the correct
                               // set of GIFs when fetching more.
  private scrollTimeout: number|null;

  static get observers() {
    return [
      'categoriesDataChanged(categoriesData.splices,lazyIndexing)',
    ];
  }

  override ready() {
    super.ready();

    // Cast here is safe since that is the spec from the cr search field mixin.
    this.addEventListener(
        'search', (ev) => this.onSearch((ev as CustomEvent<string>).detail));
    this.$.search.getSearchInput().addEventListener(
        'keydown', (ev: KeyboardEvent) => this.onSearchKeyDown(ev));
    this.addEventListener(GIF_ERROR_TRY_AGAIN, this.onClickTryAgain);
  }

  private onSearch(newSearch: string): void {
    this.set('searchResults', this.computeLocalSearchResults(newSearch));
    if (this.gifSupport) {
      this.computeInitialGifSearchResults(newSearch).then((searchResults) => {
        this.push('searchResults', ...searchResults);
      });
    }
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
   * Event handler for keydown on the search input. Used to switch focus to the
   * results list on down arrow or enter key presses.
   */
  onSearchKeyDown(ev: KeyboardEvent): void {
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

  /**
   * Computes search results for a keyword.
   *
   */
  private computeLocalSearchResults(search: string): EmojiGroupData {
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

  private onSearchScroll(): void {
    if (this.gifSupport) {
      if (this.scrollTimeout) {
        clearTimeout(this.scrollTimeout);
      }
      this.scrollTimeout = setTimeout(() => {
        this.checkScrollPosition();
      }, 100);
    }
  }

  /**
   * Checks the current scroll position and decides if new GIF elements need to
   * be fetched and displayed.
   */
  private checkScrollPosition(): void {
    const thisRect = this.shadowRoot?.getElementById('results');
    const searchResultRect = this.shadowRoot?.getElementById('search-results');

    if (!thisRect || !searchResultRect) {
      return;
    }

    // No need to append more GIFs if the first set of GIFs is still rendering.
    if (searchResultRect.getBoundingClientRect().height <=
        thisRect.getBoundingClientRect().height) {
      return;
    }

    // Append more GIFs to show if user is near the bottom of the currently
    // rendered GIFs (300px is around the average height of 2 GIFs).
    if (searchResultRect!.getBoundingClientRect().bottom -
            thisRect!.getBoundingClientRect().bottom <=
        300) {
      const gifIndex = this.searchResults.findIndex(
          group => group.category === CategoryEnum.GIF);
      if (gifIndex === -1) {
        return;
      }

      this.computeFollowingGifSearchResults(this.$.search.getValue())
          .then((searchResults) => {
            this.push(['searchResults', gifIndex, 'emoji'], ...searchResults);
          });
    }
  }

  private async computeInitialGifSearchResults(search: string):
      Promise<EmojiGroupData> {
    if (!search) {
      return [];
    }

    const searchResults: EmojiGroupData = [];
    const apiProxy = EmojiPickerApiProxyImpl.getInstance();
    const {status, searchGifs} = await apiProxy.searchGifs(search);
    this.status = status;
    this.nextGifPos = searchGifs.next;
    searchResults.push({
      'category': CategoryEnum.GIF,
      'group': '',
      'emoji': apiProxy.convertTenorGifsToEmoji(searchGifs),
      'searchOnly': false,
    });
    return searchResults;
  }

  private async computeFollowingGifSearchResults(search: string):
      Promise<EmojiVariants[]> {
    if (!search) {
      return [];
    }

    const apiProxy = EmojiPickerApiProxyImpl.getInstance();
    const {searchGifs} = await apiProxy.searchGifs(search, this.nextGifPos);
    this.nextGifPos = searchGifs.next;
    return apiProxy.convertTenorGifsToEmoji(searchGifs);
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
   * Calculates the total number of items in the search results.
   *
   */
  getNumSearchResults(): number {
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
   * Display no results if `gifSupport` flag is off and `searchResults` are
   * empty. If `gifSupport` flag is on it will always have gifs to display.
   */
  noResults(searchResults: EmojiGroupData): boolean {
    return !this.gifSupport && searchResults.length === 0;
  }

  isGifInErrorState(status: Status, searchResults: EmojiGroupData): boolean {
    return this.gifSupport && status !== Status.kHttpOk &&
        searchResults.length === 0;
  }

  onClickTryAgain() {
    this.onSearch(this.$.search.getValue());
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

declare global {
  interface HTMLElementTagNameMap {
    [EmojiSearch.is]: EmojiSearch;
  }
}


customElements.define(EmojiSearch.is, EmojiSearch);
