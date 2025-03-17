// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TooltipPosition} from '//resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DescriptionCitationElement} from './description_citation.js';
import {getAbbreviatedUrl} from './utils.js';

export function getHtml(this: DescriptionCitationElement) {
  // clang-format off
  return html`
  <cr-button id="citation" class="tonal-button"
      @click="${this.openCitation_}"
      aria-label="${this.getAriaLabel_()}">
    ${this.index}
  </cr-button><cr-tooltip id="tooltip" for="citation"
      position="${TooltipPosition.TOP}" offset="0" animation-delay="0"
      fit-to-visible-bounds>
    <div class="citation">
      <div class="header">
        ${this.urlInfo.faviconUrl.url ? html`
              <div class="faviconContainer">
                <img is="cr-auto-img" auto-src="${this.urlInfo.faviconUrl.url}">
              </div>` : ''}
        <div class="url">${getAbbreviatedUrl(this.urlInfo.url.url)}</div>
      </div>
      <div class="title">${this.urlInfo.title}</div>
      ${this.urlInfo.previewText ? html`
          <div class="previewText">${this.urlInfo.previewText}</div>` : ''}
    </div>
  </cr-tooltip>`;
  // clang-format on
}
