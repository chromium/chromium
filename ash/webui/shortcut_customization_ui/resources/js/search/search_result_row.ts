// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {FocusRowMixin} from 'chrome://resources/cr_elements/focus_row_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MojoSearchResult} from 'js/shortcut_types.js';

import {mojoString16ToString} from '../mojo_utils.js';

import {getTemplate} from './search_result_row.html.js';

/**
 * @fileoverview
 * 'search-result-row' is the container for one search result.
 */

const SearchResultRowElementBase = FocusRowMixin(PolymerElement);

export class SearchResultRowElement extends SearchResultRowElementBase {
  static get is(): string {
    return 'search-result-row';
  }

  static get properties(): PolymerElementProperties {
    return {
      // TODO(longbowei): This is an incomplete type. Update it in the future.
      searchResult: {
        type: Object,
      },

      /** Whether the search result row is selected. */
      selected: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  searchResult: MojoSearchResult;
  selected: boolean;

  private getSearchResultText(): string {
    return mojoString16ToString(
        this.searchResult.acceleratorLayoutInfo.description);
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-result-row': SearchResultRowElement;
  }
}

customElements.define(SearchResultRowElement.is, SearchResultRowElement);
