// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_search_field/cr_search_field.js';
import './emoji_category_button.js';
import './emoji_group.js';

import {CrSearchFieldElement} from 'chrome://resources/ash/common/cr_elements/cr_search_field/cr_search_field.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {Size} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerSpliceChange} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NO_INTERNET_SEARCH_ERROR_MSG} from './constants.js';
import {EmojiPickerApiProxy} from './emoji_picker_api_proxy.js';
import {getTemplate} from './emoji_search.html.js';
import {createCustomEvent, EMOJI_IMG_BUTTON_CLICK, GIF_ERROR_TRY_AGAIN} from './events.js';
import Fuse from './fuse.js';
import {Status} from './tenor_types.mojom-webui.js';
import {CategoryData, CategoryEnum, EmojiGroupData, EmojiVariants, Gender, Tone} from './types.js';

declare global {
  interface HTMLElementTagNameMap {
    'seal-snackbar': { show(): void } & HTMLElement;
  }
}

interface Image {
  url: Url;
  size: Size;
}

export interface EmojiSearch {
  $: {
    search: CrSearchFieldElement,
    searchShadow: HTMLElement,
  };
}

const SEAL_DEFAULT_STYLE_NAME = 'seal';

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
      sealSupport: {type: Boolean, value: false},
      status: {type: Status, value: null},
      searchQuery: {type: String, value: ''},
      nextGifPos: {type: String, value: ''},
      errorMessage: {type: String, value: NO_INTERNET_SEARCH_ERROR_MSG},
      closeGifNudgeOverlay: {type: Object},
      useMojoSearch: {type: Boolean, value: false},
      useGroupedPreference: {type: Boolean, value: false},
      globalTone: {type: Number, value: null, readonly: true},
      globalGender: {type: Number, value: null, readonly: true},
      sealMode: {type: Boolean, value: false},
    };
  }
  categoriesData: EmojiGroupData;
  categoryMetadata: CategoryData[];
  lazyIndexing: boolean;
  private searchResults: EmojiGroupData;
  private needIndexing: boolean;
  private gifSupport: boolean;
  private sealSupport: boolean;
  private status: Status|null;
  private closeGifNudgeOverlay: () => void;
  private useMojoSearch = false;
  private useGroupedPreference: boolean;
  private globalTone: Tone|null = null;
  private globalGender: Gender|null = null;
  private sealMode: boolean;

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

  private async onSearch(newSearch: string): Promise<void> {
    this.sealMode = this.isSealMode(newSearch);
    if (this.sealMode) {
      return;
    }

    const localSearchResults = this.useMojoSearch ?
        await this.computeEmojiSearchResults(newSearch) :
        this.computeLocalSearchResults(newSearch);

    if (!this.gifSupport) {
      this.set('searchResults', localSearchResults);
    } else {
      // With GIF support, we will progressively show local search results first
      // and more online GIFs after. To avoid displaying a "no results" screen in
      // the middle, we only do this update when local search results are not
      // empty.
      if (localSearchResults.length > 0) {
        this.set('searchResults', localSearchResults);
      }
      this.computeInitialGifSearchResults(newSearch).then((searchResults) => {
        this.set('searchResults', [...localSearchResults, ...searchResults]);
      });
    }

    // If the user is searching, to ensure emoji tooltip or variants popup can
    // be full displayed, we need to specify the minimum height as 100%.
    this.updateStyles({
      '--min-height': (newSearch.length > 0 ? '100%' : 'unset'),
    });
  }

  // TODO(b/281609806): Remove this compatibility logic once gif support is
  // turned on by default
  private getSearchPlaceholderLabel(gifSupport: boolean): string {
    return gifSupport ? 'Search' : 'Search emojis';
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
    // If GIF support is enabled, we may have an overlay for the GIF nudge. Need
    // to ensure the overlay is closed before searching for anything.
    this.closeGifNudgeOverlay();

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

  private findEmoji(category: CategoryEnum, emojiString: string):
      EmojiVariants {
    for (const group of this.categoriesData) {
      if (group.category !== category) {
        continue;
      }
      for (const emoji of group.emoji) {
        if (emoji.base.string === emojiString) {
          return emoji;
        }
      }
    }
    assertNotReached('Not able to find matching emoji');
  }

  private async computeEmojiSearchResults(search: string):
      Promise<EmojiGroupData> {
    const results = await EmojiPickerApiProxy.getInstance().searchEmoji(search);

    return [
      {
        category: CategoryEnum.EMOJI,
        group: '',
        emoji: results.emojiResults.results.map(
            (emoji) => this.findEmoji(CategoryEnum.EMOJI, emoji)),
      },
      {
        category: CategoryEnum.SYMBOL,
        group: '',
        emoji: results.symbolResults.results.map(
            (emoji) => this.findEmoji(CategoryEnum.SYMBOL, emoji)),
      },
      {
        category: CategoryEnum.EMOTICON,
        group: '',
        emoji: results.emoticonResults.results.map(
            (emoji) => this.findEmoji(CategoryEnum.EMOTICON, emoji)),
      },
    ];
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

      // As part of loading more GIFs process, we also show seal snackbar.
      if (!this.sealMode && this.sealSupport) {
        this.shadowRoot?.querySelector('seal-snackbar')?.show();
      }
    }
  }

  private async computeInitialGifSearchResults(search: string):
      Promise<EmojiGroupData> {
    if (!search) {
      return [];
    }

    const searchResults: EmojiGroupData = [];
    const apiProxy = EmojiPickerApiProxy.getInstance();
    const {status, searchGifs} = await apiProxy.searchGifs(search);
    this.status = status;
    this.nextGifPos = searchGifs.next;

    if (searchGifs.results.length > 0) {
      searchResults.push({
        'category': CategoryEnum.GIF,
        'group': '',
        'emoji': apiProxy.convertTenorGifsToEmoji(searchGifs),
        'searchOnly': false,
      });
    }

    return searchResults;
  }

  private async computeFollowingGifSearchResults(search: string):
      Promise<EmojiVariants[]> {
    if (!search) {
      return [];
    }

    const apiProxy = EmojiPickerApiProxy.getInstance();
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

  noResults(status: Status, searchResults: EmojiGroupData): boolean {
    return (!this.gifSupport || status === Status.kHttpOk) &&
        searchResults.length === 0;
  }

  isGifInErrorState(status: Status, searchResults: EmojiGroupData): boolean {
    return this.gifSupport && status !== Status.kHttpOk &&
        searchResults.length === 0;
  }

  onClickTryAgain() {
    this.onSearch(this.$.search.getValue());
  }

  getSearchQuery(): string {
    return this.$.search.getValue();
  }

  isSealMode(query: string): boolean {
    return query.includes(':');
  }

  onSealToastConfirmed() {
    if (!this.sealMode && this.sealSupport) {
      this.setSearchQuery(`${SEAL_DEFAULT_STYLE_NAME}: ${this.getSearchQuery()}`);
    }
  }

  onSealQueryChange(e: CustomEvent<string>) {
    this.setSearchQuery(e.detail);
  }

  onSealImageClick(e: CustomEvent<Image>) {
    this.dispatchEvent(createCustomEvent(
        EMOJI_IMG_BUTTON_CLICK,
        {
          name: 'image',
          category: CategoryEnum.GIF,
          visualContent: {
            id: 'seal',
            url: {
              full: e.detail.url,
              preview: e.detail.url,
              previewImage: e.detail.url,
            },
            previewSize: e.detail.size,
          },
        },
        ));
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
