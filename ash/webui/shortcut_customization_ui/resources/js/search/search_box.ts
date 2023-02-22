// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import './search_result_row.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MojoSearchResult, ShortcutSearchHandlerInterface} from '../shortcut_types.js';

import {getTemplate} from './search_box.html.js';
import {getShortcutSearchHandler} from './shortcut_search_handler.js';

/**
 * @fileoverview
 * 'search-box' is the container for the search input and shortcut search
 * results.
 */
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
    };
  }

  searchResults: MojoSearchResult[];
  shouldShowDropdown: boolean;
  private shortcutSearchHandler: ShortcutSearchHandlerInterface;

  constructor() {
    super();
    this.shortcutSearchHandler = getShortcutSearchHandler();
  }

  // TODO(longbowei): Query the search results as user is typing. Add some
  // debouncing to the search input in the future.
  protected onKeyDown(e: KeyboardEvent): void {
    if (e.key === 'Enter') {
      this.shouldShowDropdown = true;
      this.getSearchResult();
    }
  }

  protected getSearchResult(): void {
    this.shortcutSearchHandler.search().then((result) => {
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
