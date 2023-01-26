// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export interface SearchCompanionAppElement {
  $: {};
}

export class SearchCompanionAppElement extends PolymerElement {
  static get is() {
    return 'search-companion-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'search-companion-app': SearchCompanionAppElement;
  }
}
customElements.define(SearchCompanionAppElement.is, SearchCompanionAppElement);