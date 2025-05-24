// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SpListItemBadgeElement} from './sp_list_item_badge.js';

export function getHtml(this: SpListItemBadgeElement) {
  return html`
<slot></slot>
<slot name="previous-badge"></slot>`;
}
