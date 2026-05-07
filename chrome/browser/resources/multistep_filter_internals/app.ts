// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';

export class MultistepFilterInternalsAppElement extends CrLitElement {
  static get is() {
    return 'multistep-filter-internals-app';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'multistep-filter-internals-app': MultistepFilterInternalsAppElement;
  }
}

customElements.define(
    MultistepFilterInternalsAppElement.is, MultistepFilterInternalsAppElement);
