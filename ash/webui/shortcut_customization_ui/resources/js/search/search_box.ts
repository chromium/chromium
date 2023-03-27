// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import './search_result_row.js';
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {stringToMojoString16} from '../mojo_utils.js';
import {MojoSearchResult, ShortcutSearchHandlerInterface} from '../shortcut_types.js';

import {getTemplate} from './search_box.html.js';
import {SearchResultRowElement} from './search_result_row.js';
import {getShortcutSearchHandler} from './shortcut_search_handler.js';

/**
 * @fileoverview
 * 'search-box' is the container for the search input and shortcut search
 * results.
 */

// TODO(longbowei): This value is temporary. Update it once more information is
// provided.
const MAX_NUM_RESULTS = 5;

const SearchBoxElementBase = I18nMixin(PolymerElement);

export class SearchBoxElement extends SearchBoxElementBase {
  static get is(): string {
    return 'search-box';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      searchResults: {
        type: Array,
        value: [],
        observer: SearchBoxElement.prototype.onSearchResultsChanged,
      },

      shouldShowDropdown: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      searchResultsExist: {
        type: Boolean,
        value: false,
        computed: 'computeSearchResultsExist(searchResults)',
      },

      hasSearchQuery: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * The currently selected search result associated with a
       * <search-result-row>. This property is bound to the <iron-list>. Note
       * that when an item is selected, its associated <search-result-row>
       * is not focus()ed at the same time unless it is explicitly
       * clicked/tapped.
       */
      selectedItem: {
        type: Object,
      },

      /**
       * Used by FocusRowMixin to track the last focused element inside a
       * <search-result-row> with the attribute 'focus-row-control'.
       */
      lastFocused: Object,

      /**
       * Used by FocusRowMixin to track if the list has been blurred.
       */
      listBlurred: Boolean,
    };
  }

  searchResults: MojoSearchResult[];
  shouldShowDropdown: boolean;
  hasSearchQuery: true;
  private lastFocused: HTMLElement|null;
  private listBlurred: boolean;
  private searchResultsExist: boolean;
  private selectedItem: MojoSearchResult;
  private shortcutSearchHandler: ShortcutSearchHandlerInterface;

  constructor() {
    super();
    this.shortcutSearchHandler = getShortcutSearchHandler();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('blur', this.onBlur);
    this.addEventListener('keydown', this.onKeyDown);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    const searchInput =
        strictQuery('#search', this.shadowRoot, CrToolbarSearchFieldElement)
            .getSearchInput();
    searchInput.addEventListener('focus', this.onSearchInputFocused.bind(this));
    searchInput.addEventListener(
        'mousedown', this.onSearchInputMousedown.bind(this));
  }

  private onBlur(event: UIEvent): void {
    event.stopPropagation();
    // Close the dropdown because a region outside the search box was clicked.
    this.shouldShowDropdown = false;
  }

  private computeSearchResultsExist(): boolean {
    return this.searchResults.length !== 0;
  }

  private getCurrentQuery(): string {
    return strictQuery('#search', this.shadowRoot, CrToolbarSearchFieldElement)
        .getSearchInput()
        .value;
  }

  /**
   * Returns the correct tab index since <iron-list>'s default tabIndex property
   * does not automatically add selectedItem's <search-result-row> to the
   * default navigation flow, unless the user explicitly clicks on the row.
   * @param item The search result item in searchResults.
   * @return A 0 if the row should be in the navigation flow, or a -1
   *     if the row should not be in the navigation flow.
   */
  private getRowTabIndex(item: MojoSearchResult): number {
    return this.isItemSelected(item) && this.shouldShowDropdown ? 0 : -1;
  }

  private onSearchIconClicked(): void {
    // Select the query text.
    strictQuery('#search', this.shadowRoot, CrToolbarSearchFieldElement)
        .getSearchInput()
        .select();

    if (this.getCurrentQuery()) {
      this.shouldShowDropdown = true;
    }
  }

  private onSearchInputFocused(): void {
    if (this.searchResultsExist) {
      // Restore previous results instead of re-fetching.
      this.shouldShowDropdown = true;
      return;
    }

    this.fetchSearchResults(this.getCurrentQuery());
  }

  private onSearchInputMousedown(): void {
    // If the search input is clicked while the dropdown is closed, and there
    // already contains input text from a previous query, highlight the entire
    // query text so that the user can choose to easily replace the query
    // instead of having to delete the previous query manually. A mousedown
    // event is used because it is captured before |shouldShowDropdown|
    // changes, unlike a click event which is captured after
    // |shouldShowDropdown| changes.
    if (!this.shouldShowDropdown) {
      // Select all search input text once the initial state is set.
      const searchInput =
          strictQuery('#search', this.shadowRoot, CrToolbarSearchFieldElement)
              .getSearchInput();
      afterNextRender(this, () => searchInput.select());
    }
  }

  private onKeyDown(e: KeyboardEvent): void {
    // TODO(cambickel): Query the search results as user is typing. Add some
    // debouncing to the search input in the future.
    if (e.key === 'Enter') {
      this.shouldShowDropdown = true;
      const query: string = this.getCurrentQuery();
      this.hasSearchQuery = true;
      this.fetchSearchResults(query);
      return;
    }

    if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
      // Do not impact the position of <cr-toolbar-search-field>'s caret.
      e.preventDefault();
      this.selectRowViaKeys(e.key);
      return;
    }
  }

  private onSearchResultsChanged(): void {
    // Select the first search result if it exists.
    if (this.searchResultsExist) {
      this.selectedItem = this.searchResults[0];
    }

    // Only show dropdown if focus is on search field with a non empty query.
    this.shouldShowDropdown =
        strictQuery('#search', this.shadowRoot, CrToolbarSearchFieldElement)
            .isSearchFocused() &&
        !!this.getCurrentQuery();

    if (this.shouldShowDropdown && !this.searchResultsExist) {
      getAnnouncerInstance().announce(this.i18n('searchNoResults'));
      return;
    }
  }

  /**
   * @param item The search result item in searchResults.
   * @return True if the item is selected.
   */
  private isItemSelected(item: MojoSearchResult): boolean {
    return this.searchResults.indexOf(item) ===
        this.searchResults.indexOf(this.selectedItem);
  }

  /**
   * @return The <search-result-row> that is associated with the selectedItem.
   */
  private getSelectedSearchResultRow(): SearchResultRowElement {
    return strictQuery(
        'search-result-row[selected]',
        strictQuery('#searchResultList', this.shadowRoot, HTMLElement),
        SearchResultRowElement);
  }

  /**
   * @param key The string associated with a key.
   */
  private selectRowViaKeys(key: string): void {
    assert(key === 'ArrowDown' || key === 'ArrowUp', 'Only arrow keys.');
    assert(!!this.selectedItem, 'There should be a selected item already.');

    // Select the new item.
    const selectedRowIndex = this.searchResults.indexOf(this.selectedItem);
    const numRows = this.searchResults.length;
    const delta = key === 'ArrowUp' ? -1 : 1;
    const indexOfNewRow = (numRows + selectedRowIndex + delta) % numRows;
    this.selectedItem = this.searchResults[indexOfNewRow];

    // If this.lastFocused is truthy, that means a row was previously focused.
    if (this.lastFocused) {
      // If a row was previously focused, focus the currently selected row.
      // Calling focus() on a <search-result-row> focuses the element within
      // containing the attribute 'focus-row-control'.
      this.getSelectedSearchResultRow().focus();
    }

    // TODO(cambickel): Scroll into view if needed.
    // The newly selected item might not be visible because the list needs
    // to be scrolled. So scroll the dropdown if necessary.
  }

  private fetchSearchResults(query: string): void {
    if (query === '') {
      this.searchResults = [];
      return;
    }

    this.shortcutSearchHandler
        .search(stringToMojoString16(query), MAX_NUM_RESULTS)
        .then((result) => {
          this.searchResults = result.results;
          this.dispatchEvent(new CustomEvent(
              'search-results-fetched', {bubbles: true, composed: true}));
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-box': SearchBoxElement;
  }
}

customElements.define(SearchBoxElement.is, SearchBoxElement);
