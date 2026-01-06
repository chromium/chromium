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
      <cr-chip class="chip" ?disabled="${this.isEditingViaInput(item)}"
          ?selected="${!this.isEditingViaInput(item)}" role="button"
          aria-haspopup="dialog" data-filter-category="${item}"
          @click="${this.onChipClick}" @keydown="${this.onChipKeydown}">
        ${this.getFilterLabel(item)}
        <cr-icon-button iron-icon="cr:close" data-filter-category="${item}"
            @click="${this.onRemoveFilterClick}"
            aria-label="$i18n{removeFilter}">
        </cr-icon-button>
      </cr-chip>
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
      </filter-dialog-type>
    ` : ''}
    ${this.filterMenuState === FilterCategory.APP ? html`
      <app-dialog .anchorElement="${this.getDialogAnchor()}"
          .initialSelections="${this.filterSettings.apps}"
          @filter-change="${this.onAppFilterChange}"
          @close="${this.onFilterDialogClose}">
      </filter-dialog-app>
    ` : ''}
    ${this.filterMenuState === FilterCategory.EVENT ? html`
      <event-dialog .anchorElement="${this.getDialogAnchor()}"
          .initialSelections="${
              this.filterSettings.eventTypes}"
          @filter-change="${this.onEventTypeFilterChange}"
          @close="${this.onFilterDialogClose}">
      </filter-dialog-event>
    ` : ''}
    ${this.filterMenuState === FilterCategory.OUTCOME ? html`
      <outcome-dialog .anchorElement="${this.getDialogAnchor()}"
          .initialSelections="${
              this.filterSettings.updateOutcomes}"
          @filter-change="${this.onUpdateOutcomeFilterChange}"
          @close="${this.onFilterDialogClose}">
      </filter-dialog-outcome>
    ` : ''}
    ${this.filterMenuState === FilterCategory.SCOPE ? html`
      <scope-dialog .anchorElement="${this.getDialogAnchor()}"
          .initialSelections="${this.filterSettings.scopes}"
          @filter-change="${this.onScopeFilterChange}"
          @close="${this.onFilterDialogClose}">
      </filter-dialog-scope>
    ` : ''}
    ${this.filterMenuState === FilterCategory.DATE ? html`
      <date-dialog .anchorElement="${this.getDialogAnchor()}"
          .initialStartDate="${this.filterSettings.startDate}"
          .initialEndDate="${this.filterSettings.endDate}"
          @filter-change="${this.onDateFilterChange}"
          @close="${this.onFilterDialogClose}">
      </filter-dialog-date>
    ` : ''}
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
