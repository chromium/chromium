// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivateStateTokensNavigationElement} from './navigation.js';
import {ItemsToRender} from './types.js';

export function getHtml(this: PrivateStateTokensNavigationElement) {
  // clang-format off
  return html`
${this.itemToRender === ItemsToRender.ISSUER_METADATA ? html`
  <private-state-tokens-metadata
      .issuerOrigin="${this.metadata.issuerOrigin}"
      .expiration="${this.metadata.expiration}"
      .purposes="${this.metadata.purposes}">
  </private-state-tokens-metadata>
`: ''}

${this.itemToRender === ItemsToRender.ISSUER_LIST ? html`
  <private-state-tokens-list-container id="pst-container"
      .data="${this.data}">
  </private-state-tokens-list-container>
` : ''}`;
  // clang-format on
}
