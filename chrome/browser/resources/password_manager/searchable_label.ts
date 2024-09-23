// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {quoteString} from 'chrome://resources/js/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class SearchableLabelElement extends PolymerElement {
  static get is() {
    return 'searchable-label';
  }

  static get template() {
    return null;
  }

  static get properties() {
    return {
      // The text to show in this label.
      title: String,

      // The search term to bold within the title.
      searchTerm: String,
    };
  }

  searchTerm: string;

  static get observers() {
    return ['setSearchedTextToBold_(title, searchTerm)'];
  }

  /**
   * Updates the page title. If a search term is specified, highlights any
   * occurrences of the search term in bold.
   */
  private setSearchedTextToBold_() {
    if (this.title === undefined) {
      return;
    }

    const titleText = this.title;

    if (!this.searchTerm) {
      this.textContent = ` ${titleText} `;
      return;
    }

    const re = new RegExp(quoteString(this.searchTerm), 'gim');
    let i = 0;
    let match;
    this.textContent = '';
    while (match = re.exec(titleText)) {
      if (match.index > i) {
        this.appendTextElement('span', titleText.slice(i, match.index));
      }
      i = re.lastIndex;
      // Mark the highlighted text in bold.
      this.appendTextElement('b', titleText.substring(match.index, i));
    }
    if (i < titleText.length) {
      this.appendTextElement('span', titleText.slice(i));
    }
  }

  private appendTextElement(type: string, text: string) {
    const element = document.createElement(type);
    element.textContent = text;
    this.appendChild(element);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'searchable-label': SearchableLabelElement;
  }
}

customElements.define(SearchableLabelElement.is, SearchableLabelElement);
