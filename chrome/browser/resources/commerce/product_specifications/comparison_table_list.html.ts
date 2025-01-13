// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './comparison_table_list_item.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComparisonTableListElement} from './comparison_table_list.js';

export function getHtml(this: ComparisonTableListElement) {
  return html`<!--_html_template_start_-->
  <div id="header">
    $i18n{yourComparisonTables}
    <cr-icon-button id="edit" iron-icon="product-specifications:edit"
        @click="${this.onEditClick_}">
    </cr-icon-button>
  </div>

  <div id="listContainer">
    <cr-lazy-list id="list" .scrollTarget="${this}" .items="${this.tables}">
      ${this.tables.map(table => html`
        <comparison-table-list-item
          name="${table.name}"
          .uuid="${table.uuid}"
          num-urls="${table.numUrls}"
          .imageUrl="${table.imageUrl}"
          ?has-checkbox="${this.isEditing_}"
          @checkbox-change="${this.onCheckboxChange_}"
          @delete-table="${this.stopEditing_}">
        </comparison-table-list-item>`)}
    </cr-lazy-list>
  </div>

  <div id="footer" ?hidden="${!this.isEditing_}">
    <cr-toolbar-selection-overlay id="toolbar"
        cancel-label="$i18n{cancelA11yLabel}"
        selection-label="${this.getSelectionLabel_(this.numSelected_)}"
        @clear-selected-items="${this.onClearClick_}"
        ?show="${this.isEditing_}">
      <div class="sp-icon-buttons-row">
        <cr-icon-button id="delete" iron-icon="cr:delete"
            title="$i18n{menuDelete}" aria-label="$i18n{menuDelete}"
            ?disabled="${this.deletePending_ || this.numSelected_ === 0}"
            @click="${this.onDeleteClick_}">
        </cr-icon-button>
      </div>
    </cr-toolbar-selection-overlay>
  </div>
  <!--_html_template_end_-->`;
}
