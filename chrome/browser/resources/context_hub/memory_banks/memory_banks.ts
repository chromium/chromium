// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './memory_banks.html.js';

export class MemoryBanksElement extends CrLitElement {
  static get is() {
    return 'memory-banks';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-banks': MemoryBanksElement;
  }
}

customElements.define(MemoryBanksElement.is, MemoryBanksElement);
