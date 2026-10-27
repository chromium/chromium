// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {quoteString} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

export class SearchableLabelElement extends CrLitElement {
  static get is() {
    return 'searchable-label';
  }

  static override get properties() {
    return {
      // The text to show in this label.
      title: {type: String},

      // The search term to bold within the title.
      searchTerm: {type: String},
    };
  }

  override accessor title: string = '';
  accessor searchTerm: string = '';

  // Do not attach a shadowRoot so that nodes appended to the light DOM in
  // `setSearchedTextToBold_()` are actually visible.
  override createRenderRoot() {
    return this;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('title') || changedProperties.has('searchTerm')) {
      this.setSearchedTextToBold_();
    }
  }

  /**
   * Updates the page title. If a search term is specified, highlights any
   * occurrences of the search term in bold.
   */
  private setSearchedTextToBold_() {
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
