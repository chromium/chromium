// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-search-box' is the container for the search input
 * and settings search results.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import 'chrome://resources/js/focus_row.js';
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../icons.html.js';
import '../settings_shared.css.js';
import './os_search_result_row.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {recordSearch} from '../metrics_recorder.js';
import {SearchResultsObserverInterface as PersonalizationSearchResultsObserverInterface, SearchResultsObserverReceiver as PersonalizationSearchResultsObserverReceiver} from '../mojom-webui/personalization_search.mojom-webui.js';
import {ParentResultBehavior, SearchResultsObserverInterface, SearchResultsObserverReceiver} from '../mojom-webui/search.mojom-webui.js';
import {Router, routes} from '../router.js';
import {combinedSearch, getPersonalizationSearchHandler, getSettingsSearchHandler, SearchResult} from '../search/combined_search_handler.js';

import {OsSearchResultRowElement} from './os_search_result_row.js';
import {getTemplate} from './os_settings_search_box.html.js';
import {OsSettingsSearchBoxBrowserProxy, OsSettingsSearchBoxBrowserProxyImpl} from './os_settings_search_box_browser_proxy.js';

const MAX_NUM_SEARCH_RESULTS = 5;

const SEARCH_REQUEST_METRIC_NAME = 'ChromeOS.Settings.SearchRequests';

const USER_ACTION_ON_SEARCH_RESULTS_SHOWN_METRIC_NAME =
    'ChromeOS.Settings.UserActionOnSearchResultsShown';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 */
enum OsSettingSearchRequestTypes {
  ANY_SEARCH_REQUEST = 0,
  DISCARED_RESULTS_SEARCH_REQUEST = 1,
  SHOWN_RESULTS_SEARCH_REQUEST = 2,
}

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 */
enum OsSettingSearchBoxUserAction {
  SEARCH_RESULT_CLICKED = 0,
  CLICKED_OUT_OF_SEARCH_BOX = 1,
}

export interface OsSettingsSearchBoxElement {
  $: {
    search: CrToolbarSearchFieldElement,
    searchResultList: IronListElement,
  };
}

const OsSettingsSearchBoxElementBase = I18nMixin(PolymerElement);

export class OsSettingsSearchBoxElement extends OsSettingsSearchBoxElementBase
    implements SearchResultsObserverInterface,
               PersonalizationSearchResultsObserverInterface {
  static get is() {
    return 'os-settings-search-box';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // True when the toolbar is displaying in narrow mode.
      // TODO(hsuregan): Change narrow to isNarrow here and associated elements.
      narrow: {
        type: Boolean,
        reflectToAttribute: true,
      },

      // Controls whether the search field is shown.
      showingSearch: {
        type: Boolean,
        value: false,
        notify: true,
        reflectToAttribute: true,
      },

      hasSearchQuery: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // Value is proxied through to cr-toolbar-search-field. When true,
      // the search field will show a processing spinner.
      spinnerActive: Boolean,

      /**
       * The currently selected search result associated with an
       * <os-search-result-row>. This property is bound to the <iron-list>. Note
       * that when an item is selected, its associated <os-search-result-row>
       * is not focus()ed at the same time unless it is explicitly
       * clicked/tapped.
       */
      selectedItem_: {
        type: Object,
      },

      /**
       * Prevent user deselection by tracking last item selected. This item must
       * only be assigned to an item within |this.$.searchResultList|, and not
       * directly to |this.selectedItem_| or an item within
       * |this.searchResults_|.
       */
      lastSelectedItem_: {
        type: Object,
      },

      /**
       * Passed into <iron-list>. Exactly one result is the selectedItem_.
       */
      searchResults_: {
        type: Array,
        value: [],
        observer: 'onSearchResultsChanged_',
      },

      shouldShowDropdown_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      searchResultsExist_: {
        type: Boolean,
        value: false,
        computed: 'computeSearchResultsExist_(searchResults_)',
      },

      shouldHideFeedbackButton_: {
        type: Boolean,
        computed: 'computeShouldHideFeedbackButton_(' +
            'hasSearchQuery, searchResultsExist_)',
      },

      /**
       * Used by FocusRowMixin to track the last focused element inside a
       * <os-search-result-row> with the attribute 'focus-row-control'.
       */
      lastFocused_: Object,

      /**
       * Used by FocusRowMixin to track if the list has been blurred.
       */
      listBlurred_: Boolean,

      /**
       * The number of searches performed in one lifecycle of the search box.
       */
      searchRequestCount_: {
        type: Number,
        value: 0,
      },
    };
  }

  narrow: boolean;
  showingSearch: boolean;
  hasSearchQuery: boolean;
  spinnerActive: boolean;
  private selectedItem_: SearchResult;
  private lastSelectedItem_: SearchResult|null;
  private searchResults_: SearchResult[];
  private shouldHideFeedbackButton_: boolean;
  private shouldShowDropdown_: boolean;
  private searchResultsExist_: boolean;
  private lastFocused_: HTMLElement|null;
  private listBlurred_: boolean;
  private searchRequestCount_: number;

  private settingsSearchResultObserverReceiver_: SearchResultsObserverReceiver|
      null;
  private personalizationSearchResultObserverReceiver_:
      PersonalizationSearchResultsObserverReceiver|null;
  private osSettingsSearchBoxBrowserProxy_: OsSettingsSearchBoxBrowserProxy;

  constructor() {
    super();

    this.settingsSearchResultObserverReceiver_ = null;
    this.personalizationSearchResultObserverReceiver_ = null;
    this.osSettingsSearchBoxBrowserProxy_ =
        OsSettingsSearchBoxBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('blur', this.onBlur_);
    this.addEventListener('keydown', this.onKeyDown_);
    this.addEventListener('search-changed', this.onSearchChanged_);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    const toolbarSearchField = this.$.search;
    const searchInput = toolbarSearchField.getSearchInput();
    if (Router.getInstance().currentRoute === routes.BASIC) {
      // The search field should only focus initially if settings is opened
      // directly to the base page, with no path. Opening to a section or a
      // subpage should not focus the search field.
      toolbarSearchField.showAndFocus();
    }
    searchInput.addEventListener(
        'focus', this.onSearchInputFocused_.bind(this));
    searchInput.addEventListener(
        'mousedown', this.onSearchInputMousedown_.bind(this));

    // If the search was initiated by directly entering a search URL, need to
    // sync the URL parameter to the textbox.
    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('search') || '';

    // Setting the search box value without triggering a 'search-changed'
    // event, to prevent an unnecessary duplicate entry in |window.history|.
    toolbarSearchField.setValue(urlSearchQuery, /*noEvent=*/ true);

    // Log number of search requests made each time settings window closes.
    window.addEventListener('beforeunload', () => {
      chrome.metricsPrivate.recordSparseValue(
          'ChromeOS.Settings.SearchRequestsPerSession',
          this.searchRequestCount_);
    });

    // Observe changes to personalization search results.
    this.personalizationSearchResultObserverReceiver_ =
        new PersonalizationSearchResultsObserverReceiver(this);
    getPersonalizationSearchHandler().addObserver(
        this.personalizationSearchResultObserverReceiver_.$
            .bindNewPipeAndPassRemote());

    // Observe for availability changes of settings results.
    this.settingsSearchResultObserverReceiver_ =
        new SearchResultsObserverReceiver(this);
    getSettingsSearchHandler().observe(
        this.settingsSearchResultObserverReceiver_.$
            .bindNewPipeAndPassRemote());
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    assert(
        this.personalizationSearchResultObserverReceiver_,
        'Personalization search observer should be initialized');
    this.personalizationSearchResultObserverReceiver_.$.close();

    assert(
        this.settingsSearchResultObserverReceiver_,
        'Settings search observer should be initialized');
    this.settingsSearchResultObserverReceiver_.$.close();
  }

  /**
   * Overrides SearchResultsObserverInterface
   * Overrides PersonalizationSearchResultsObserverInterface
   */
  onSearchResultsChanged(): void {
    this.fetchSearchResults_();
  }

  /**
   * @return The <os-search-result-row> that is associated with the selectedItem
   */
  private getSelectedOsSearchResultRow_(): OsSearchResultRowElement {
    return castExists(
        this.$.searchResultList.querySelector<OsSearchResultRowElement>(
            'os-search-result-row[selected]'),
        'No OsSearchResultRow is selected.');
  }

  /**
   * @return The current input string.
   */
  private getCurrentQuery_(): string {
    return this.$.search.getSearchInput().value;
  }

  private computeShouldHideFeedbackButton_(): boolean {
    return this.searchResultsExist_ || !this.hasSearchQuery;
  }

  private computeSearchResultsExist_(): boolean {
    return this.searchResults_.length !== 0;
  }

  private onSearchChanged_(): void {
    this.hasSearchQuery = this.getCurrentQuery_().trim().length !== 0;
    this.fetchSearchResults_();
  }

  private fetchSearchResults_(): void {
    const query = this.getCurrentQuery_();
    if (query === '') {
      this.searchResults_ = [];
      return;
    }

    this.spinnerActive = true;

    // The C++ layer uses std::u16string, which use 16 bit characters. JS
    // strings support either 8 or 16 bit characters, and must be converted to
    // an array of 16 bit character codes that match std::u16string.
    const queryMojoString16 = stringToMojoString16(query);
    const timeOfSearchRequest = Date.now();
    combinedSearch(
        queryMojoString16, MAX_NUM_SEARCH_RESULTS,
        ParentResultBehavior.kAllowParentResults)
        .then(response => {
          const latencyMs = Date.now() - timeOfSearchRequest;
          chrome.metricsPrivate.recordTime(
              'ChromeOS.Settings.SearchLatency', latencyMs);
          this.onSearchResultsReceived_(query, response.results);
          const event = new CustomEvent(
              'search-results-fetched', {bubbles: true, composed: true});
          this.dispatchEvent(event);
        });

    ++this.searchRequestCount_;
    chrome.metricsPrivate.recordEnumerationValue(
        SEARCH_REQUEST_METRIC_NAME,
        OsSettingSearchRequestTypes.ANY_SEARCH_REQUEST,
        Object.keys(OsSettingSearchRequestTypes).length);
    chrome.metricsPrivate.recordSparseValue(
        'ChromeOS.Settings.NumCharsOfQueries', query.length);
  }

  /**
   * Updates search results UI when settings search results are fetched.
   * @param query The string used to find search results.
   * @param results Array of search results.
   */
  private onSearchResultsReceived_(query: string, results: SearchResult[]):
      void {
    chrome.metricsPrivate.recordSparseValue(
        'ChromeOS.Settings.NumSearchResultsFetched', results.length);

    const shouldDiscardResults = query !== this.getCurrentQuery_();

    chrome.metricsPrivate.recordEnumerationValue(
        SEARCH_REQUEST_METRIC_NAME,
        shouldDiscardResults ?
            OsSettingSearchRequestTypes.DISCARED_RESULTS_SEARCH_REQUEST :
            OsSettingSearchRequestTypes.SHOWN_RESULTS_SEARCH_REQUEST,
        Object.keys(OsSettingSearchRequestTypes).length);

    if (shouldDiscardResults) {
      // Received search results are invalid as the query has since changed.
      return;
    }

    this.spinnerActive = false;
    this.lastFocused_ = null;
    this.searchResults_ = results;
    recordSearch();
  }

  private onNavigatedToResultRowRoute_(): void {
    // Blur search input to prevent blinking caret. Note that this blur event
    // will not always be propagated to OsSettingsSearchBox (e.g. user decides
    // to click on the same search result twice) so |this.shouldShowDropdown_|
    // must always be set to false in |this.onNavigatedToResultRowRoute_()|.
    this.$.search.blur();

    // Settings has navigated to another page; close search results dropdown.
    this.shouldShowDropdown_ = false;

    chrome.metricsPrivate.recordEnumerationValue(
        USER_ACTION_ON_SEARCH_RESULTS_SHOWN_METRIC_NAME,
        OsSettingSearchBoxUserAction.SEARCH_RESULT_CLICKED,
        Object.keys(OsSettingSearchBoxUserAction).length);
  }

  private onBlur_(event: UIEvent): void {
    event.stopPropagation();

    // A blur event generated programmatically (as is most of the time the case
    // in onNavigatedToResultRowRoute_()), or focusing a different window other
    // than the OS Settings window will have a null |e.sourceCapabilities|. On
    // the other hand, a blur event generated by intentionally clicking or
    // tapping an area outside the search box, but still within the OS Settings
    // window, will have a non-null |e.sourceCapabilities|.
    if (event.sourceCapabilities && this.searchResultsExist_) {
      chrome.metricsPrivate.recordEnumerationValue(
          USER_ACTION_ON_SEARCH_RESULTS_SHOWN_METRIC_NAME,
          OsSettingSearchBoxUserAction.CLICKED_OUT_OF_SEARCH_BOX,
          Object.keys(OsSettingSearchBoxUserAction).length);
    }

    // Close the dropdown because  a region outside the search box was clicked.
    this.shouldShowDropdown_ = false;
  }

  private onSearchInputFocused_(): void {
    this.lastFocused_ = null;

    if (this.searchResultsExist_) {
      // Restore previous results instead of re-fetching.
      this.shouldShowDropdown_ = true;
      return;
    }

    this.fetchSearchResults_();
  }

  private onSearchInputMousedown_(): void {
    // If the search input is clicked while the dropdown is closed, and there
    // already contains input text from a previous query, highlight the entire
    // query text so that the user can choose to easily replace the query
    // instead of having to delete the previous query manually. A mousedown
    // event is used because it is captured before |shouldShowDropdown_|
    // changes, unlike a click event which is captured after
    // |shouldShowDropdown_| changes.
    if (!this.shouldShowDropdown_) {
      // Select all search input text once the initial state is set.
      const searchInput = this.$.search.getSearchInput();
      afterNextRender(this, () => searchInput.select());
    }
  }

  /**
   * @param item The search result item in searchResults_.
   * @return True if the item is selected.
   */
  private isItemSelected_(item: SearchResult): boolean {
    return this.searchResults_.indexOf(item) ===
        this.searchResults_.indexOf(this.selectedItem_);
  }

  /**
   * @return Length of the search results array.
   */
  private getListLength_(): number {
    return this.searchResults_.length;
  }

  /**
   * Returns the correct tab index since <iron-list>'s default tabIndex property
   * does not automatically add selectedItem_'s <os-search-result-row> to the
   * default navigation flow, unless the user explicitly clicks on the row.
   * @param item The search result item in searchResults_.
   * @return A 0 if the row should be in the navigation flow, or a -1
   *     if the row should not be in the navigation flow.
   */
  private getRowTabIndex_(item: SearchResult): number {
    return this.isItemSelected_(item) && this.shouldShowDropdown_ ? 0 : -1;
  }

  private onSearchResultsChanged_(): void {
    // Select the first search result if it exists.
    if (this.searchResultsExist_) {
      this.selectedItem_ = this.searchResults_[0];
    }

    // Only show dropdown if focus is on search field with a non empty query.
    this.shouldShowDropdown_ =
        this.$.search.isSearchFocused() && !!this.getCurrentQuery_();

    if (!this.shouldShowDropdown_) {
      return;
    }

    if (!this.searchResultsExist_) {
      getAnnouncerInstance().announce(this.i18n('searchNoResults'));
      return;
    }
  }

  /**
   * |selectedItem| is not changed by the time this is called. The value that
   * |selectedItem| will be assigned to is stored in
   * |this.$.searchResultList.selectedItem|.
   */
  private onSelectedItemChanged_(): void {
    // <iron-list> causes |this.$.searchResultList.selectedItem| to be null if
    // tapped a second time.
    if (!this.$.searchResultList.selectedItem && this.lastSelectedItem_) {
      // In the case that the user deselects a search result, reselect the item
      // manually by altering the list. Setting |selectedItem| will be no use
      // as |selectedItem| has not been assigned at this point. Adding an
      // observer on |selectedItem| to address a value change will also be no
      // use as it will perpetuate an infinite select and deselect chain in this
      // case.
      this.$.searchResultList.selectItem(this.lastSelectedItem_);
    }
    this.lastSelectedItem_ =
        this.$.searchResultList.selectedItem as SearchResult;
  }

  /**
   * @param key The string associated with a key.
   */
  private selectRowViaKeys_(key: string): void {
    assert(key === 'ArrowDown' || key === 'ArrowUp', 'Only arrow keys.');
    assert(!!this.selectedItem_, 'There should be a selected item already.');

    // Select the new item.
    const selectedRowIndex = this.searchResults_.indexOf(this.selectedItem_);
    const numRows = this.searchResults_.length;
    const delta = key === 'ArrowUp' ? -1 : 1;
    const indexOfNewRow = (numRows + selectedRowIndex + delta) % numRows;
    this.selectedItem_ = this.searchResults_[indexOfNewRow];

    if (this.lastFocused_) {
      // If a row was previously focused, focus the currently selected row.
      // Calling focus() on a <os-search-result-row> focuses the element within
      // containing the attribute 'focus-row-control'.
      this.getSelectedOsSearchResultRow_().focus();
    }

    // The newly selected item might not be visible because the list needs
    // to be scrolled. So scroll the dropdown if necessary.
    this.getSelectedOsSearchResultRow_().scrollIntoViewIfNeeded();
  }

  // <if expr="_google_chrome">
  private onSendFeedbackClick_(): void {
    const descriptionTemplate =
        this.i18nAdvanced('searchFeedbackDescriptionTemplate', {
              substitutions: [this.getCurrentQuery_()],
            })
            .toString();
    this.osSettingsSearchBoxBrowserProxy_.openSearchFeedbackDialog(
        descriptionTemplate);
  }
  // </if>

  /**
   * Keydown handler to specify how enter-key, arrow-up key, and arrow-down-key
   * interacts with search results in the dropdown. Note that 'Enter' on keyDown
   * when a row is focused is blocked by FocusRowMixin.
   */
  private onKeyDown_(e: KeyboardEvent): void {
    if (!this.searchResultsExist_ ||
        (!this.$.search.isSearchFocused() && !this.lastFocused_)) {
      // No action should be taken if there are no search results, or when
      // neither the search input nor a <os-search-result-row> is focused
      // (ChromeVox may focus on clear search input button).
      return;
    }

    if (e.key === 'Enter') {
      this.getSelectedOsSearchResultRow_().onSearchResultSelected();
      return;
    }

    if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
      // Do not impact the position of <cr-toolbar-search-field>'s caret.
      e.preventDefault();
      this.selectRowViaKeys_(e.key);
      return;
    }
  }

  private onSearchIconClicked_(): void {
    this.$.search.getSearchInput().select();
    if (this.getCurrentQuery_()) {
      this.shouldShowDropdown_ = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-search-box': OsSettingsSearchBoxElement;
  }
}

customElements.define(
    OsSettingsSearchBoxElement.is, OsSettingsSearchBoxElement);
