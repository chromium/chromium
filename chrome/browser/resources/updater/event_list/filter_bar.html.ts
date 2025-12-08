// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {EventType} from '../event_history.js';
import {COMMON_UPDATE_OUTCOMES, EVENT_TYPES, localizeEventType, localizeUpdateOutcome} from '../event_history.js';
import {getKnownApps} from '../known_apps.js';

import type {FilterBarElement, FilterCategory} from './filter_bar.js';

function renderChip(
    element: FilterBarElement, category: FilterCategory, label: string,
    onEdit: () => void, onRemove: () => void) {
  const isEditingThisViaInput =
      element.filterMenuState === category && element.menuHost === 'input';
  const isEditingThisViaChip =
      element.filterMenuState === category && element.menuHost === 'chip';

  const onClick = (e: MouseEvent) => {
    // Prevent opening the menu if the close button was clicked
    if ((e.target as HTMLElement).tagName !== 'CR-ICON-BUTTON') {
      onEdit();
    }
  };
  const onKeydown = (e: KeyboardEvent) => element.handleChipKeydown(e, onEdit);

  return html`
    <div class="chip-wrapper">
      <cr-chip
        class="chip"
        ?disabled="${isEditingThisViaInput}"
        ?selected="${!isEditingThisViaInput}"
        role="button"
        aria-haspopup="dialog"
        @click="${onClick}"
        @keydown="${onKeydown}">
        ${label}
        <cr-icon-button
          iron-icon="cr:close"
          @click="${onRemove}"
          aria-label="${loadTimeData.getString('removeFilter')}">
        </cr-icon-button>
      </cr-chip>
      ${isEditingThisViaChip ? renderFilterDialog(element) : nothing}
    </div>
  `;
}

function renderCheckboxItem(
    label: string, isChecked: boolean, onChange: (e: Event) => void) {
  return html`
    <cr-checkbox
        class="filter-menu-item"
        ?checked="${isChecked}"
        @checked-changed="${onChange}">
      ${label}
    </cr-checkbox>
  `;
}

function renderFilterTypeDialogContents(element: FilterBarElement) {
  const menuItems = [
    {
      id: 'app',
      label: 'app',
      action: () => {
        element.filterMenuState = 'app';
        element.pendingAppSelections =
            new Set(element.filterSettings.activeAppFilters);
      },
    },
    {
      id: 'event',
      label: 'eventType',
      action: () => {
        element.filterMenuState = 'event';
        element.pendingEventTypeSelections =
            new Set(element.filterSettings.activeEventTypeFilters);
      },
    },
    {
      id: 'outcome',
      label: 'updateOutcome',
      action: () => {
        element.filterMenuState = 'outcome';
        element.pendingUpdateOutcomeSelections =
            new Set(element.filterSettings.activeUpdateOutcomeFilters);
      },
    },
    {
      id: 'date',
      label: 'date',
      action: () => {
        element.filterMenuState = 'date';
        element.pendingStartDateFilter = element.filterSettings.startDateFilter;
        element.pendingEndDateFilter = element.filterSettings.endDateFilter;
      },
    },
  ];

  return menuItems.map(item => {
    const onKeydown = (e: KeyboardEvent) => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        item.action();
      }
    };
    return html`
    <div
      class="filter-menu-item"
      tabindex="0"
      role="button"
      @keydown="${onKeydown}"
      @click="${item.action}">
      ${loadTimeData.getString(item.label)}
    </div>`;
  });
}

function renderFilterMenuFooter(
    element: FilterBarElement, onApply: () => void) {
  return html`
    <div class="filter-menu-footer">
      <cr-button
        class="cancel-button"
        @click="${element.closeFilterMenu}">
        ${loadTimeData.getString('cancel')}
      </cr-button>
      <cr-button
        class="action-button"
        @click="${onApply}">
        ${loadTimeData.getString('apply')}
      </cr-button>
    </div>`;
}

function renderAppFilterDialogContents(element: FilterBarElement) {
  const apps = getKnownApps();
  const predefinedApps = [...apps.keys()].filter(
      (appName) =>
          appName.toLowerCase().includes(element.appSearch.toLowerCase()),
  );
  const customApps = [...element.pendingAppSelections].filter(
      (appName) => !apps.has(appName) &&
          appName.toLowerCase().includes(element.appSearch.toLowerCase()),
  );
  const appCandidates = new Set([...predefinedApps, ...customApps]);
  if (element.appSearch && ![...apps.keys()].includes(element.appSearch) &&
      ![...element.pendingAppSelections].includes(element.appSearch)) {
    appCandidates.add(element.appSearch);
  }
  const appCheckboxes = [...appCandidates].map(
      (appName) => renderCheckboxItem(
          appName, element.pendingAppSelections.has(appName),
          (e: Event) => element.onAppCheckboxChange(e, appName)));

  const onInput = (e: InputEvent) =>
      (element.appSearch = (e.target as HTMLInputElement).value);
  return html`
    <input
      type="text"
      class="filter-menu-input"
      placeholder="${loadTimeData.getString('appNameOrId')}"
      .value="${element.appSearch}"
      @input="${onInput}"
      @keydown="${element.handleAppSearchKeydown}" />
    ${appCheckboxes}
    ${renderFilterMenuFooter(element, element.applyAppFilters)}`;
}

function renderEventTypeFilterDialogContents(element: FilterBarElement) {
  const COMMON_EVENT_TYPES: EventType[] = [
    'UPDATE',
    'INSTALL',
    'UNINSTALL',
  ];
  const OTHER_EVENT_TYPES =
      Object.values(EVENT_TYPES)
          .filter((et) => !COMMON_EVENT_TYPES.includes(et));

  const commonCheckboxes = COMMON_EVENT_TYPES.map(
      eventType => renderCheckboxItem(
          localizeEventType(eventType),
          element.pendingEventTypeSelections.has(eventType),
          (e: Event) => element.onEventTypeCheckboxChange(e, eventType)));
  const otherCheckboxes = OTHER_EVENT_TYPES.map(
      eventType => renderCheckboxItem(
          localizeEventType(eventType),
          element.pendingEventTypeSelections.has(eventType),
          (e: Event) => element.onEventTypeCheckboxChange(e, eventType)));
  return html`
    <div class="filter-menu-section-header">
      ${loadTimeData.getString('common')}
    </div>
    ${commonCheckboxes}
    <div class="filter-menu-section-header">
      ${loadTimeData.getString('other')}
    </div>
    ${otherCheckboxes}
    ${renderFilterMenuFooter(element, element.applyEventTypeFilters)}`;
}

function renderOutcomeFilterDialogContents(element: FilterBarElement) {
  const checkboxes = COMMON_UPDATE_OUTCOMES.map(
      (outcome) => renderCheckboxItem(
          localizeUpdateOutcome(outcome),
          element.pendingUpdateOutcomeSelections.has(outcome),
          (e: Event) => element.onUpdateOutcomeCheckboxChange(e, outcome)));
  return html`
    ${checkboxes}
    ${renderFilterMenuFooter(element, element.applyUpdateOutcomeFilters)}`;
}

function renderDateFilterDialogContents(element: FilterBarElement) {
  const startTime = element.pendingStartDateFilter ?
      element.pendingStartDateFilter.getTime() :
      NaN;
  const onStartTimeInput = (e: Event) => {
    const value = (e.target as HTMLInputElement).valueAsNumber;
    element.pendingStartDateFilter = isNaN(value) ? null : new Date(value);
  };
  const endTime = element.pendingEndDateFilter ?
      element.pendingEndDateFilter.getTime() :
      NaN;
  const onEndTimeInput = (e: Event) => {
    const value = (e.target as HTMLInputElement).valueAsNumber;
    element.pendingEndDateFilter = isNaN(value) ? null : new Date(value);
  };
  return html`
    <div class="filter-menu-date-inputs">
      <label for="start-date">
        ${loadTimeData.getString('startDate')}
      </label>
      <input
        type="datetime-local"
        id="start-date"
        .valueAsNumber="${startTime}"
        @input="${onStartTimeInput}" />
      <label for="end-date">
        ${loadTimeData.getString('endDate')}
      </label>
      <input
        type="datetime-local"
        id="end-date"
        .valueAsNumber="${endTime}"
        @input="${onEndTimeInput}" />
    </div>
    ${renderFilterMenuFooter(element, element.applyDateFilters)}`;
}

function renderFilterDialog(element: FilterBarElement) {
  const content = (() => {
    switch (element.filterMenuState) {
      case 'closed':
        return nothing;
      case 'type':
        return renderFilterTypeDialogContents(element);
      case 'app':
        return renderAppFilterDialogContents(element);
      case 'event':
        return renderEventTypeFilterDialogContents(element);
      case 'outcome':
        return renderOutcomeFilterDialogContents(element);
      case 'date':
        return renderDateFilterDialogContents(element);
    }
  })();

  return html`
    <div
      class="filter-menu"
      role="dialog"
      @keydown="${element.handleMenuKeydown}">
      ${content}
    </div>
  `;
}

export function getHtml(this: FilterBarElement) {
  const chipTemplates: {[key in FilterCategory]?: () => TemplateResult} = {
    'date': () => renderChip(
        this, 'date',
        loadTimeData.getStringF('filterChipDate', this.getDateFilterString()),
        () => this.editDateFilter(),
        () => {
          this.updateFilterOrder('date', false);
          this.filterSettings.startDateFilter = null;
          this.filterSettings.endDateFilter = null;
          this.dispatchFiltersChanged();
        }),
    'app': () => renderChip(
        this, 'app',
        loadTimeData.getStringF(
            'filterChipApp',
            [...this.filterSettings.activeAppFilters].join(', ')),
        () => this.editAppFilter(),
        () => {
          this.updateFilterOrder('app', false);
          this.filterSettings.activeAppFilters = new Set();
          this.dispatchFiltersChanged();
        }),
    'event': () => renderChip(
        this,
        'event',
        loadTimeData.getStringF(
            'filterChipEventType',
            [...this.filterSettings.activeEventTypeFilters]
                .map(localizeEventType)
                .join(', ')),
        () => this.editEventTypeFilter(),
        () => {
          this.updateFilterOrder('event', false);
          this.filterSettings.activeEventTypeFilters = new Set();
          this.dispatchFiltersChanged();
        }),
    'outcome': () => renderChip(
        this,
        'outcome',
        loadTimeData.getStringF(
            'filterChipUpdateOutcome',
            [...this.filterSettings.activeUpdateOutcomeFilters]
                .map(localizeUpdateOutcome)
                .join(', ')),
        () => this.editUpdateOutcomeFilter(),
        () => {
          this.updateFilterOrder('outcome', false);
          this.filterSettings.activeUpdateOutcomeFilters = new Set();
          this.dispatchFiltersChanged();
        }),
  };

  const shouldRenderFilterDialog =
      this.filterMenuState !== 'closed' && this.menuHost === 'input';
  const clearAllFiltersButton = html`
    <button
      id="clear-filters-button"
      @click="${this.clearAllFilters}"
      aria-label="${loadTimeData.getString('clearAllFilters')}">
      &times;
    </button>`;

  return html`
    <div class="filter-bar">
      <cr-icon icon="updater:filter"></cr-icon>
      ${this.filterOrder.map((filter) => chipTemplates[filter]!())}
      <div id="filter-bar-input-container" class="chip-wrapper">
        <cr-button
          id="add-filter-button"
          aria-haspopup="dialog"
          @click="${this.openFilterMenu}">
          ${loadTimeData.getString('addFilter')}
        </cr-button>
        ${shouldRenderFilterDialog ? renderFilterDialog(this) : nothing}
      </div>
      ${this.filterOrder.length > 0 ? clearAllFiltersButton : nothing}
    </div>
    `;
}
