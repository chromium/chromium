// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

class SuggestInternalsAppElement extends PolymerElement {
  static get is() {
    return 'suggest-internals-app';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'suggest-internals-app': SuggestInternalsAppElement;
  }
}

customElements.define(
    SuggestInternalsAppElement.is, SuggestInternalsAppElement);
