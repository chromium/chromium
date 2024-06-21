// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivateStateTokensAppElement} from './app.js';

export function getHtml(this: PrivateStateTokensAppElement) {
  return html`
  <private-state-tokens-toolbar
      id="toolbar"
      .pageName="${this.pageTitle_}"
      .narrow="${this.narrow_}">
  </private-state-tokens-toolbar>`;
}
