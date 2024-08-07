// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivateStateTokensNavigationElement} from './navigation.js';
import {ItemsToRender} from './types.js';

export function getHtml(this: PrivateStateTokensNavigationElement) {
  if (this.itemToRender === ItemsToRender.ISSUER_METADATA) {
    //clang-format off
    return html`<private-state-tokens-metadata
        .issuerOrigin=${this.metadata_.issuerOrigin}
        .expiration=${this.metadata_.expiration}
        .purposes=${this.metadata_.purposes}></private-state-tokens-metadata>`;
  }
  if (this.itemToRender === ItemsToRender.ISSUER_LIST) {
    return html`<private-state-tokens-list-container id="pst-container" .data=${
        this.data}>
    </private-state-tokens-list-container>`;
  }
  throw new Error(`Unexpected item to render: ${this.itemToRender}`);
  //clang-format on
}
