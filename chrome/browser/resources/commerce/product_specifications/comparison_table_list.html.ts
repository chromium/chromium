// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './comparison_table_list_item.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComparisonTableListElement} from './comparison_table_list.js';

export function getHtml(this: ComparisonTableListElement) {
  return html`<!--_html_template_start_-->
  <div id="header">$i18n{yourComparisonTables}</div>
  <div id="listContainer">
    <cr-lazy-list id="list" .scrollTarget="${this}" .items="${this.tables}">
      ${this.tables.map(table => html`
        <comparison-table-list-item
          name="${table.name}"
          .uuid="${table.uuid}"
          num-urls="${table.numUrls}"
          .imageUrl="${table.imageUrl}">
        </comparison-table-list-item>`)}
    </cr-lazy-list>
  </div>
  <!--_html_template_end_-->`;
}
