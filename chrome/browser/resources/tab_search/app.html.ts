// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabSearchAppElement} from './app.js';

export function getHtml(this: TabSearchAppElement) {
  // clang-format off
  return html`
<tab-search-page available-height="${this.availableHeight_}">
</tab-search-page>`;
  // clang-format on
}
