// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { CustomElement } from 'chrome://resources/js/custom_element.js';

import {Router} from './router.js';
import { getTemplate } from './search_bar.html.js';

export class SearchBarElement extends CustomElement {
  static get is() {
    return 'search-bar';
  }

  static override get template() {
    return getTemplate();
  }

  private searchInput_: HTMLInputElement;

  constructor() {
    super();
    this.searchInput_ =
        this.getRequiredElement<HTMLInputElement>('#search-input');
  }

  connectedCallback() {
    this.searchInput_.addEventListener('input', () => {
      this.onSearchInput();
    });
  }

  /**
   * Sets the search input's value. This is used to programmatically
   * update the search bar's text, for example when a query is
   * present in the URL on page load.
   */
  setQuery(query: string) {
    if (this.searchInput_.value !== query) {
      this.searchInput_.value = query;
    }
  }

  /**
   * Sets focus on the search input element. This is used to maintain focus
   * after a search causes the UI to re-render, preventing the user from
   * having to click the search bar again to continue typing.
   */
  focusInput() {
    this.searchInput_.focus();
  }

  private onSearchInput() {
    Router.getInstance().setSearchQuery(this.searchInput_.value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-bar': SearchBarElement;
  }
}

customElements.define('search-bar', SearchBarElement);
