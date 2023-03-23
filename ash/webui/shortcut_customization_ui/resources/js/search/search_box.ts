// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import './search_result_row.js';
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {stringToMojoString16} from '../mojo_utils.js';
import {MojoSearchResult, ShortcutSearchHandlerInterface} from '../shortcut_types.js';

import {getTemplate} from './search_box.html.js';
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
    };
  }

  searchResults: MojoSearchResult[];
  shouldShowDropdown: boolean;
  hasSearchQuery: true;
  private shortcutSearchHandler: ShortcutSearchHandlerInterface;

  constructor() {
    super();
    this.shortcutSearchHandler = getShortcutSearchHandler();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('blur', this.onBlur);
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

  // TODO(longbowei): Query the search results as user is typing. Add some
  // debouncing to the search input in the future.
  protected onKeyDown(e: KeyboardEvent): void {
    if (e.key === 'Enter') {
      this.shouldShowDropdown = true;
      const query: string = this.getCurrentQuery();
      this.hasSearchQuery = true;
      this.fetchSearchResults(query);
    }
  }

  protected fetchSearchResults(query: string): void {
    this.shortcutSearchHandler
        .search(stringToMojoString16(query), MAX_NUM_RESULTS)
        .then((result) => {
          this.searchResults = result.results;
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-box': SearchBoxElement;
  }
}

customElements.define(SearchBoxElement.is, SearchBoxElement);
