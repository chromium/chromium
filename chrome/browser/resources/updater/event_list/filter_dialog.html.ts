// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {COMMON_UPDATE_OUTCOMES, localizeEventType, localizeUpdateOutcome} from '../event_history.js';

import {FilterCategory} from './filter_bar.js';
import type {FilterDialogElement} from './filter_dialog.js';

export function getHtml(this: FilterDialogElement) {
  // clang-format off
  return html`
  <!--_html_template_start_-->
  <div class="filter-menu" role="dialog" @keydown="${this.onMenuKeydown}">
    ${this.type === 'type' ? html`
      ${this.filterMenuItems.map(item => html`
        <div class="filter-menu-item" tabindex="0" role="button"
            data-filter-category="${item.filterCategory}"
            @keydown="${this.onTypeMenuKeydown}"
            @click="${this.onTypeMenuClick}">
          ${item.label}
        </div>
      `)}
    ` : ''}
    ${this.type === FilterCategory.APP ? html`
      <input type="text" class="filter-menu-input"
          placeholder="$i18n{appNameOrId}" .value="${this.appSearch}"
          @input="${this.onAppSearchInput}"
          @keydown="${this.onAppSearchKeydown}">
      ${this.displayedApps.map(item => html`
        <cr-checkbox class="filter-menu-item"
            ?checked="${this.pendingAppSelections.has(item)}"
            data-app-name="${item}"
            @checked-changed="${this.onAppCheckboxCheckedChanged}">
          ${item}
        </cr-checkbox>
      `)}
      <div class="filter-menu-footer">
        <cr-button class="cancel-button" @click="${this.onCancelClick}">
          $i18n{cancel}
        </cr-button>
        <cr-button class="action-button"
            @click="${this.onAppApplyClick}">
          $i18n{apply}
        </cr-button>
      </div>
    ` : ''}
    ${this.type === FilterCategory.EVENT ? html`
      <div class="filter-menu-section-header">$i18n{common}</div>
      ${this.commonEventTypes.map(item => html`
        <cr-checkbox class="filter-menu-item"
            ?checked="${this.pendingEventTypeSelections.has(item)}"
            data-event-type="${item}"
            @checked-changed="${this.onEventTypeCheckboxCheckedChanged}">
          ${localizeEventType(item)}
        </cr-checkbox>
      `)}
      <div class="filter-menu-section-header">$i18n{other}</div>
      ${this.otherEventTypes.map(item => html`
        <cr-checkbox class="filter-menu-item"
            ?checked="${this.pendingEventTypeSelections.has(item)}"
            data-event-type="${item}"
            @checked-changed="${this.onEventTypeCheckboxCheckedChanged}">
          ${localizeEventType(item)}
        </cr-checkbox>
      `)}
      <div class="filter-menu-footer">
        <cr-button class="cancel-button" @click="${this.onCancelClick}">
          $i18n{cancel}
        </cr-button>
        <cr-button class="action-button"
            @click="${this.onEventTypeApplyClick}">
          $i18n{apply}
        </cr-button>
      </div>
    ` : ''}
    ${this.type === FilterCategory.OUTCOME ? html`
      ${COMMON_UPDATE_OUTCOMES.map(item => html`
        <cr-checkbox class="filter-menu-item"
            ?checked="${this.pendingUpdateOutcomeSelections.has(item)}"
            data-outcome="${item}"
            @checked-changed="${this.onUpdateOutcomeCheckboxCheckedChanged}">
          ${localizeUpdateOutcome(item)}
        </cr-checkbox>
      `)}
      <div class="filter-menu-footer">
        <cr-button class="cancel-button" @click="${this.onCancelClick}">
          $i18n{cancel}
        </cr-button>
        <cr-button class="action-button"
            @click="${this.onOutcomeApplyClick}">
          $i18n{apply}
        </cr-button>
      </div>
    ` : ''}
    ${this.type === FilterCategory.DATE ? html`
      <div class="filter-menu-date-inputs">
        <label for="start-date">$i18n{startDate}</label>
        <input type="datetime-local" id="start-date"
            .valueAsNumber="${this.pendingStartDate}"
            @input="${this.onStartTimeInput}">
        <label for="end-date">$i18n{endDate}</label>
        <input type="datetime-local" id="end-date"
            .valueAsNumber="${this.pendingEndDate}"
            @input="${this.onEndTimeInput}">
      </div>
      <div class="filter-menu-footer">
        <cr-button class="cancel-button" @click="${this.onCancelClick}">
          $i18n{cancel}
        </cr-button>
        <cr-button class="action-button"
            @click="${this.onDateApplyClick}">
          $i18n{apply}
        </cr-button>
      </div>
    ` : ''}
  </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
