// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MojoSearchResult} from 'js/shortcut_types.js';

import {mojoString16ToString} from '../mojo_utils.js';

import {getTemplate} from './search_result_row.html.js';

/**
 * @fileoverview
 * 'search-result-row' is the container for one search result.
 */

export class SearchResultRowElement extends PolymerElement {
  static get is(): string {
    return 'search-result-row';
  }

  static get properties(): PolymerElementProperties {
    return {
      // TODO(longbowei): This is an incomplete type. Update it in the future.
      searchResult: {
        type: Object,
      },
    };
  }

  searchResult: MojoSearchResult;

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
