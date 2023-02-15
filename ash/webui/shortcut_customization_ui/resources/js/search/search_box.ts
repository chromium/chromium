// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './search_box.html.js';

/**
 * @fileoverview
 * 'search-box' is the container for the search input and shortcut search
 * results.
 */
export class SearchBoxElement extends PolymerElement {
  static get is(): string {
    return 'search-box';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-box': SearchBoxElement;
  }
}

customElements.define(SearchBoxElement.is, SearchBoxElement);
