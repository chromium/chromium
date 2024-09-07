// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DescriptionSectionElement} from './description_section.js';

export function getHtml(this: DescriptionSectionElement) {
  return html`
  <div id="attributes">
    ${
      this.description.attributes.map(
          (attrItem) => html`
      <div class="attribute-chip">
        ${
              attrItem.label &&
              html`<span class="attribute-title">${attrItem.label}</span>:`}
        ${attrItem.value}
      </div>
    `)}
  </div>
  <div id="summary">
    ${this.description.summary.map((summaryItem, summaryIndex) => html`
      <span class="summary-text">${summaryItem.text}</span>
      ${summaryItem.urls.map((urlInfo, urlIndex) => html`
        <description-citation url="${urlInfo.url.url}"
            index="${this.computeCitationIndex_(summaryIndex, urlIndex)}"
            citation-count="${this.citationCount}"
            product-name="${this.productName}">
        </description-citation>
      `)}
    `)}
  </div>`;
}
