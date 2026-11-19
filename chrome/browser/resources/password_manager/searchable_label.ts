// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {quoteString} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './searchable_label.html.js';

export interface TextSegment {
  text: string;
  bold: boolean;
}

export class SearchableLabelElement extends CrLitElement {
  static get is() {
    return 'searchable-label';
  }

  override render() {
    return getHtml.bind(this)();
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

  // Do not attach a shadowRoot so that rendered nodes are in the light DOM.
  override createRenderRoot() {
    return this;
  }

  /**
   * Splits `title` into segments, marking occurrences of `searchTerm` as bold.
   */
  protected getSegments_(): TextSegment[] {
    assert(this.searchTerm);
    const segments: TextSegment[] = [];
    const titleText = this.title;
    const re = new RegExp(quoteString(this.searchTerm), 'gim');
    let i = 0;
    let match;
    while (match = re.exec(titleText)) {
      if (match.index > i) {
        segments.push({text: titleText.slice(i, match.index), bold: false});
      }
      i = re.lastIndex;
      // Mark the highlighted text in bold.
      segments.push({text: titleText.substring(match.index, i), bold: true});
    }
    if (i < titleText.length) {
      segments.push({text: titleText.slice(i), bold: false});
    }
    return segments;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'searchable-label': SearchableLabelElement;
  }
}

customElements.define(SearchableLabelElement.is, SearchableLabelElement);
