// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {quoteString} from 'chrome://resources/js/util.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

export class HistorySearchedLabelElement extends CrLitElement {
  static get is() {
    return 'history-searched-label';
  }

  override render() {
    return html`<slot></slot>`;
  }

  static override get properties() {
    return {
      // The text to show in this label.
      title: {type: String},

      // The search term to bold within the title.
      searchTerm: {type: String},
    };
  }

  accessor searchTerm: string;
  override accessor title: string;

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
    if (this.title === undefined) {
      return;
    }

    const titleText = this.title;

    if (this.searchTerm === '' || this.searchTerm === null ||
        this.searchTerm === undefined) {
      this.textContent = titleText;
      return;
    }

    const re = new RegExp(quoteString(this.searchTerm), 'gim');
    let i = 0;
    let match;
    this.textContent = '';
    while (match = re.exec(titleText)) {
      if (match.index > i) {
        this.appendChild(
            document.createTextNode(titleText.slice(i, match.index)));
      }
      i = re.lastIndex;
      // Mark the highlighted text in bold.
      const b = document.createElement('b');
      b.textContent = titleText.substring(match.index, i);
      this.appendChild(b);
    }
    if (i < titleText.length) {
      this.appendChild(document.createTextNode(titleText.slice(i)));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-searched-label': HistorySearchedLabelElement;
  }
}

customElements.define(
    HistorySearchedLabelElement.is, HistorySearchedLabelElement);
