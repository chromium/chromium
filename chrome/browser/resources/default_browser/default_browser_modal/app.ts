// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';


export class DefaultBrowserModalAppElement extends CrLitElement {
  static get is() {
    return 'default-browser-modal-app';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'default-browser-modal-app': DefaultBrowserModalAppElement;
  }
}

customElements.define(
    DefaultBrowserModalAppElement.is, DefaultBrowserModalAppElement);
