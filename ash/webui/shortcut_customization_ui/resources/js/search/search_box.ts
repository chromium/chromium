// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import './search_result_row.js';
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {IronDropdownElement} from 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchResultsAvailabilityObserverInterface, SearchResultsAvailabilityObserverReceiver} from '../../mojom-webui/search.mojom-webui.js';
import {AcceleratorState, MojoSearchResult, ShortcutSearchHandlerInterface} from '../shortcut_types.js';
import {isCustomizationAllowed} from '../shortcut_utils.js';

import {getTemplate} from './search_box.html.js';
import {SearchResultRowElement} from './search_result_row.js';
import {getShortcutSearchHandler} from './shortcut_search_handler.js';

/**
 * @fileoverview
 * 'search-box' is the container for the search input and shortcut search
 * results.
 */

const MAX_NUM_RESULTS = 5;
// This number was chosen arbitrarily to be a reasonable limit. Most
// searches will not be anywhere close to this.
const MAX_QUERY_LENGTH_CHARACTERS = 200;

const SearchBoxElementBase = I18nMixin(PolymerElement);

export class SearchBoxElement extends SearchBoxElementBase implements
    SearchResultsAvailabilityObserverInterface {
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

      /**
       * Value is proxied through to cr-toolbar-search-field. When true, the
       * search field will show a processing spinner.
       */
      spinnerActive: Boolean,
    };
  }

  hasSearchQuery: boolean;
  searchResults: MojoSearchResult[];
  shouldShowDropdown: boolean;
  private lastFocused: HTMLElement|null;
  private listBlurred: boolean;
  private resizeObserver: ResizeObserver;
  private searchInputElement: HTMLInputElement;
  private searchResultsExist: boolean;
  private selectedItem: MojoSearchResult;
  private shortcutSearchHandler: ShortcutSearchHandlerInterface;
  private spinnerActive: boolean;

  constructor() {
    super();
    this.shortcutSearchHandler = getShortcutSearchHandler();
    const receiver = new SearchResultsAvailabilityObserverReceiver(this);
    this.shortcutSearchHandler.addSearchResultsAvailabilityObserver(
        receiver.$.bindNewPipeAndPassRemote());
  }

  onSearchResultsAvailabilityChanged(): void {
    this.onSearchChanged();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('blur', this.onBlur);
    this.addEventListener('keydown', this.onKeyDown);
    // This event is fired (after a short debounce) from the
    // cr-toolbar-search-field when the input changes.
    this.addEventListener('search-changed', this.onSearchChanged);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    const searchFieldElement =
        strictQuery('#search', this.shadowRoot, CrToolbarSearchFieldElement);
    searchFieldElement.addEventListener(
        'transitionend', this.onSearchFieldTransitionEnd.bind(this));

    this.searchInputElement = searchFieldElement.getSearchInput();

    // Focus the search bar when the app opens.
    afterNextRender(this, () => {
      this.searchInputElement.focus();
    });

    this.searchInputElement.addEventListener(
        'focus', this.onSearchInputFocused.bind(this));
    this.searchInputElement.addEventListener(
        'mousedown', this.onSearchInputMousedown.bind(this));

    this.searchInputElement.maxLength = MAX_QUERY_LENGTH_CHARACTERS;

    // This is a required work around to get the iron-list to display correctly
    // on the first search query. Currently iron-list won't generate item
    // elements on attach if the element is not visible. To work around this, we
    // listen for resize events and manually call notifyResize on the iron-list
    // when the iron-dropdown state changes.
    this.resizeObserver = new ResizeObserver(() => {
      const ironListElement =
          (this.shadowRoot?.querySelector('iron-list') as IronListElement);
      if (ironListElement) {
        ironListElement.notifyResize();
      }
    });
    this.resizeObserver.observe(
        strictQuery('iron-dropdown', this.shadowRoot, HTMLElement));
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.resizeObserver.disconnect();
  }

  private onBlur(event: UIEvent): void {
    event.stopPropagation();
    // Close the dropdown because a region outside the search box was clicked.
    this.shouldShowDropdown = false;
  }

  private computeSearchResultsExist(): boolean {
    return this.searchResults.length !== 0;
  }

  /**
   * @return Length of the search results array.
   */
  private getListLength(): number {
    return this.searchResults.length;
  }

  private getCurrentQuery(): string {
    return this.searchInputElement.value;
  }

  private onSearchChanged(): void {
    this.hasSearchQuery = !!this.getCurrentQuery();
    if (!this.hasSearchQuery) {
      // Cancel the spinner if the current query is empty to avoid a rare case
      // where the spinner stays active forever.
      this.spinnerActive = false;
    }
    this.fetchSearchResults(this.getCurrentQuery());
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
    this.searchInputElement.select();

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
      afterNextRender(this, () => this.searchInputElement.select());
    }
  }

  private onSearchFieldTransitionEnd(): void {
    // Cast to IronDropdownElement since the interface cannot be used as a
    // value.
    const ironDropdown =
        (strictQuery('iron-dropdown', this.shadowRoot, HTMLElement) as
         IronDropdownElement);

    // Resize the dropdown once the search bar has finishing resizing to avoid
    // misalignment when the window resizes.
    ironDropdown.notifyResize();
  }

  private onKeyDown(e: KeyboardEvent): void {
    const isSearchFocused =
        strictQuery('#search', this.shadowRoot, CrToolbarSearchFieldElement)
            .isSearchFocused();
    if (!this.searchResultsExist || !(isSearchFocused || this.lastFocused)) {
      // No action should be taken if there are no search results, or when
      // neither the search input nor a <search-result-row> is focused
      // (ChromeVox may focus on clear search input button).
      return;
    }

    // Press enter to navigate to the selected search result.
    // Check that a selected search result exists first, since it's possible for
    // the user to press enter before the iron-list is fully rendered.
    if (e.key === 'Enter' && this.hasSelectedSearchResultRow()) {
      this.getSelectedSearchResultRow().onSearchResultSelected();
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
    }
  }

  private onNavigatedToResultRowRoute(): void {
    // Blur search input to prevent blinking caret. Note that this blur event
    // will not always be propagated to the SearchBoxElement (e.g. user decides
    // to click on the same search result twice) so |this.shouldShowDropdown|
    // must always be set to false in |this.onNavigatedToResultRowRoute()|.
    strictQuery('#search', this.shadowRoot, CrToolbarSearchFieldElement).blur();

    // Shortcuts has navigated to another page; close search results dropdown.
    this.shouldShowDropdown = false;
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
   * @return True if there is a selected <search-result-row> element.
   */
  private hasSelectedSearchResultRow(): boolean {
    return !!this.shadowRoot?.querySelector('search-result-row[selected]');
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

    this.spinnerActive = true;

    // In some cases, the backend will return search results that are later
    // filtered out by `this.filterSearchResults`. When that happens, the UI
    // should still show MAX_NUM_RESULTS results if there are other matching
    // results. To achieve this, we request more results than we need, and then
    // cap the number of search results to MAX_NUM_RESULTS.
    const maxNumberOfSearchResults = MAX_NUM_RESULTS * 3;

    this.shortcutSearchHandler
        .search(stringToMojoString16(query), maxNumberOfSearchResults)
        .then((response) => {
          this.onSearchResultsReceived(query, response.results);
          this.dispatchEvent(new CustomEvent(
              'search-results-fetched', {bubbles: true, composed: true}));
        });
  }

  private onSearchResultsReceived(query: string, results: MojoSearchResult[]):
      void {
    if (query !== this.getCurrentQuery()) {
      // Received search results are invalid as the query has since changed.
      return;
    }

    this.spinnerActive = false;

    this.searchResults = this.filterSearchResults(results);

    // In `this.fetchSearchResults`, we queried for a multiple of
    // MAX_NUM_RESULTS, so cap the size of the results here after filtering.
    this.searchResults = this.searchResults.slice(0, MAX_NUM_RESULTS);

    // This invalidates whatever SearchResultRow element was previously focused,
    // since it's likely that the element has been removed after the search.
    this.lastFocused = null;
  }

  /**
   * Filter the given search results to hide accelerators and results that are
   * disabled because their keys are unavailable or they are disabled by user.
   * This filtering matches the behavior of the Shortcut app's main list of
   * shortcuts.
   * @param searchResults the search results to filter.
   * @returns the given search results with disabled keys and results with no
   *     keys filtered out.
   */
  private filterSearchResults(searchResults: MojoSearchResult[]):
      MojoSearchResult[] {
    const enabledSearchResults =
        searchResults
            // Hide accelerators that are disabled because the keys are
            // unavailable.
            .map(result => ({
                   ...result,
                   acceleratorInfos: result.acceleratorInfos.filter(
                       a => a.state !==
                               AcceleratorState.kDisabledByUnavailableKeys &&
                           a.state !== AcceleratorState.kDisabledByUser),
                 }));

    // If customization is not allowed, hide results that don't contain any
    // accelerators.
    if (!isCustomizationAllowed()) {
      return enabledSearchResults.filter(
          result => result.acceleratorInfos.length > 0);
    }

    return enabledSearchResults;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-box': SearchBoxElement;
  }
}

customElements.define(SearchBoxElement.is, SearchBoxElement);
