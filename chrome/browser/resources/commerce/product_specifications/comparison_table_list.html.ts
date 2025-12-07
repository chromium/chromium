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
    <cr-lazy-list id="list" .scrollTarget="${this}"
        .items="${this.getTables_(this.tables, this.tablesPendingDeletion_)}">
      ${
      this.getTables_(this.tables, this.tablesPendingDeletion_)
          .map(table => html`
          <comparison-table-list-item
            name="${table.name}"
            .uuid="${table.uuid}"
            .urls="${table.urls}"
            ?has-checkbox="${this.isEditing_}"
            @checkbox-change="${this.onCheckboxChange_}"
            @delete-table="${this.onItemDelete_}">
          </comparison-table-list-item>`)}
    </cr-lazy-list>
  </div>

  <cr-lazy-render-lit id="toast"
      .template="${() => html`
        <cr-toast duration="${this.deletionToastDurationMs_}">
          <div>${this.deletionToastMessage_}</div>
          <cr-button id="undo" @click="${this.onUndoDeletionClick_}">
            $i18n{undoTableDeletion}
          </cr-button>
        </cr-toast>`}">
  </cr-lazy-render-lit>

  <div id="footer" ?hidden="${!this.isEditing_}">
    <cr-toolbar-selection-overlay id="toolbar"
        cancel-label="$i18n{cancelA11yLabel}"
        selection-label="${this.getSelectionLabel_(this.numSelected_)}"
        @clear-selected-items="${this.onClearClick_}"
        ?show="${this.isEditing_}">
      <div class="sp-icon-buttons-row">
        <cr-icon-button id="delete" iron-icon="cr:delete"
            title="$i18n{menuDelete}" aria-label="$i18n{menuDelete}"
            ?disabled="${this.numSelected_ === 0}"
            @click="${this.onDeleteClick_}">
        </cr-icon-button>
        <cr-icon-button id="more" iron-icon="cr:more-vert"
            title="$i18n{menuTooltipMore}" aria-label="$i18n{menuTooltipMore}"
            ?disabled="${this.numSelected_ === 0}"
            @click="${this.onShowContextMenuClick_}">
      </cr-icon-button>
      </div>
    </cr-toolbar-selection-overlay>
  </div>

  <cr-lazy-render-lit id="menu"
      .template="${() => html`
        <cr-action-menu>
          <button id="menuOpenAll" class="dropdown-item"
              @click="${this.onOpenAllClick_}">
          ${this.getOpenAllString_(this.numSelected_)}
          </button>
          <button id="menuOpenAllInNewWindow" class="dropdown-item"
              @click="${this.onOpenAllInNewWindowClick_}">
            ${this.getOpenAllInNewWindowString_(this.numSelected_)}
          </button>
          <hr>
          <button id="menuDelete" class="dropdown-item"
              @click="${this.onDeleteClick_}">
            $i18n{menuDelete}
          </button>
        </cr-action-menu>`}">
  </cr-lazy-render-lit>
  <!--_html_template_end_-->`;
}
