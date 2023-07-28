// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export class DefaultBrowserAppElement extends PolymerElement {
  static get is() {
    return 'default-browser-app';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'default-browser-app': DefaultBrowserAppElement;
  }
}

customElements.define(DefaultBrowserAppElement.is, DefaultBrowserAppElement);
