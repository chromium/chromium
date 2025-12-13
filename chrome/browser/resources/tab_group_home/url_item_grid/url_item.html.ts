// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {UrlItemElement} from './url_item.js';

export function getHtml(this: UrlItemElement) {
  return html`
    <div class="header">
      <div id="favicon" style="background-image: ${this.faviconImageSet_}">
      </div>
      <div id="title">${this.item.title}</div>
      <cr-icon-button id="closeButton" iron-icon="cr:close" title="$i18n{close}"
          @click="${this.onCloseButtonClick_}"></cr-icon-button>
    </div>
    <div class="thumbnail-container">
      <img id="thumbnail" aria-hidden="true">
    </div>
  `;
}
