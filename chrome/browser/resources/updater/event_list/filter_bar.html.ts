// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FilterBarElement} from './filter_bar.js';

export function getHtml(this: FilterBarElement) {
  // clang-format off
  return html`
  <!--_html_template_start_-->
  <div class="filter-bar">
    <cr-icon icon="updater:filter"></cr-icon>
    ${this.filterOrder.map(item => html`
      <div class="chip-wrapper">
        <cr-chip class="chip" ?disabled="${this.isEditingViaInput(item)}"
            ?selected="${!this.isEditingViaInput(item)}" role="button"
            aria-haspopup="dialog" data-filter-category="${item}"
            @click="${this.onChipClick}"
            @keydown="${this.onChipKeydown}">
          ${this.getFilterLabel(item)}
          <cr-icon-button iron-icon="cr:close"
              data-filter-category="${item}"
              @click="${this.onRemoveFilterClick}"
              aria-label="$i18n{removeFilter}">
          </cr-icon-button>
        </cr-chip>
        ${this.isEditingViaChip(item) ? html`
          <filter-dialog .type="${this.filterMenuState}"
              .filterSettings="${this.filterSettings}"
              @type-selection-changed="${this.onTypeSelectionChanged}"
              @app-filter-changed="${this.onAppFilterChanged}"
              @event-filter-changed="${this.onEventTypeFilterChanged}"
              @outcome-filter-changed="${this.onUpdateOutcomeFilterChanged}"
              @date-filter-changed="${this.onDateFilterChanged}"
              @close="${this.onFilterDialogClose}">
          </filter-dialog>
        ` : ''}
      </div>
    `)}

    <div id="filterBarInputContainer" class="chip-wrapper">
      <cr-button id="add-filter-button" aria-haspopup="dialog"
          @click="${this.onAddFilterClick}">
        $i18n{addFilter}
      </cr-button>
      ${this.filterMenuState !== 'closed' && this.menuHost === 'input' ? html`
        <filter-dialog .type="${this.filterMenuState}"
            .filterSettings="${this.filterSettings}"
            @type-selection-changed="${this.onTypeSelectionChanged}"
            @app-filter-changed="${this.onAppFilterChanged}"
            @event-filter-changed="${this.onEventTypeFilterChanged}"
            @outcome-filter-changed="${this.onUpdateOutcomeFilterChanged}"
            @date-filter-changed="${this.onDateFilterChanged}"
            @close="${this.onFilterDialogClose}">
        </filter-dialog>
      ` : ''}
    </div>
    ${this.filterOrder.length > 0 ? html`
      <button id="clear-filters-button" @click="${this.onClearFiltersClick}"
          aria-label="$i18n{clearAllFilters}">
        &times;
      </button>
    ` : ''}
  </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
