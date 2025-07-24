// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { CustomElement } from 'chrome://resources/js/custom_element.js';

import { getTemplate } from './search_bar.html.js';

export class SearchBarElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  static get is() {
    return 'search-bar';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-bar': SearchBarElement;
  }
}

customElements.define(SearchBarElement.is, SearchBarElement);