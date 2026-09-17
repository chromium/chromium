// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SmartSearchCardElement} from './smart_search_card.js';

export function getHtml(this: SmartSearchCardElement) {
  return html`
    ${
      this.result?.title && this.result?.url ?
          html`
      <cr-checkbox
          class="card-checkbox"
          .checked="${this.selected}"
          @click="${this.onCheckboxClick_}"
          @change="${this.onCheckboxChange_}">
      </cr-checkbox>
      <div class="card-main">
        <div class="card-header-row">
          <div class="favicon" style="background-image: ${
              getFaviconForPageURL(this.result.url, false)}"></div>
          <a class="card-title" href="${this.result.url}" target="_blank">
            ${this.result.title}
          </a>
        </div>
        <p class="card-snippet">${this.result.snippet || ''}</p>
      </div>
      <div class="card-actions">
        <cr-icon-button
            iron-icon="cr:open-in-new"
            title="Open URL"
            @click="${this.onOpenUrlClick_}">
        </cr-icon-button>
      </div>
    ` :
          nothing}
  `;
}
