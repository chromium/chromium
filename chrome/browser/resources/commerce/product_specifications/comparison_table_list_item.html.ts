// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComparisonTableListItemElement} from './comparison_table_list_item.js';

export function getHtml(this: ComparisonTableListItemElement) {
  // clang-format off
  return html`
  <div id="itemContainer">
    <cr-url-list-item
        id="item"
        size="large"
        title="${this.getTitle_()}"
        url="${this.tableUrl_.url}"
        description="${this.tableUrl_.url}"
        .imageUrls="${this.imageUrl ? [this.imageUrl?.url] : []}"
        @click="${() => this.fire('comparison-table-list-item-click', {
          uuid: this.uuid,
        })}">
      <div id="numItems" slot="badges">${this.numItemsString_}</div>
    </cr-url-list-item>
  </div>`;
  // clang-format on
}
