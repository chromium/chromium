// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {FilterCategory} from './filter_bar.js';
import type {FilterBarElement} from './filter_bar.js';

export function getHtml(this: FilterBarElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<div class="filter-bar">
  <cr-icon icon="updater:filter"></cr-icon>
  ${this.filterOrder.map(item => html`
    <div class="chip-wrapper">
      <button class="chip" ?disabled="${this.isEditingViaInput(item)}"
          data-filter-category="${item}" @click="${this.onChipClick}"
          aria-haspopup="dialog">
        <div class="hover-layer"></div>
        ${this.getFilterLabel(item)}
        <cr-ripple></cr-ripple>
      </button>
      <cr-icon-button iron-icon="cr:close" data-filter-category="${item}"
          ?disabled="${this.isEditingViaInput(item)}"
          @click="${this.onRemoveFilterClick}"
          aria-label="$i18n{removeFilter}">
      </cr-icon-button>
    </div>
  `)}

  <div id="filterBarInputContainer" class="chip-wrapper">
    <cr-button id="add-filter-button" aria-haspopup="dialog"
        @click="${this.onAddFilterClick}">
      $i18n{addFilter}
    </cr-button>
  </div>
  ${this.filterOrder.length > 0 ? html`
    <button id="clear-filters-button" @click="${this.onClearFiltersClick}"
        aria-label="$i18n{clearAllFilters}">
      &times;
    </button>
  ` : ''}

  ${this.isEditing() ? html`
    ${this.filterMenuState === 'type' ? html`
      <type-dialog .anchorElement="${this.getDialogAnchor()}"
          @type-selection-changed="${this.onTypeSelectionChanged}"
          @close="${this.onFilterDialogClose}">
      </type-dialog>
    ` : ''}
    ${this.filterMenuState === FilterCategory.APP ? html`
      <app-dialog .anchorElement="${this.getDialogAnchor()}"
          .initialSelections="${this.filterSettings.apps}"
          @filter-change="${this.onAppFilterChange}"
          @close="${this.onFilterDialogClose}">
      </app-dialog>
    ` : ''}
    ${this.filterMenuState === FilterCategory.EVENT ? html`
      <event-dialog .anchorElement="${this.getDialogAnchor()}"
          .initialSelections="${
              this.filterSettings.eventTypes}"
          @filter-change="${this.onEventTypeFilterChange}"
          @close="${this.onFilterDialogClose}">
      </event-dialog>
    ` : ''}
    ${this.filterMenuState === FilterCategory.OUTCOME ? html`
      <outcome-dialog .anchorElement="${this.getDialogAnchor()}"
          .initialSelections="${
              this.filterSettings.updateOutcomes}"
          @filter-change="${this.onUpdateOutcomeFilterChange}"
          @close="${this.onFilterDialogClose}">
      </outcome-dialog>
    ` : ''}
    ${this.filterMenuState === FilterCategory.SCOPE ? html`
      <scope-dialog .anchorElement="${this.getDialogAnchor()}"
          .initialSelections="${this.filterSettings.scopes}"
          @filter-change="${this.onScopeFilterChange}"
          @close="${this.onFilterDialogClose}">
      </scope-dialog>
    ` : ''}
    ${this.filterMenuState === FilterCategory.DATE ? html`
      <date-dialog .anchorElement="${this.getDialogAnchor()}"
          .initialStartDate="${this.filterSettings.startDate}"
          .initialEndDate="${this.filterSettings.endDate}"
          @filter-change="${this.onDateFilterChange}"
          @close="${this.onFilterDialogClose}">
      </date-dialog>
    ` : ''}
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
