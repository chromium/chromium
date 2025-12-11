// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {assert, assertNotReached, assertNotReachedCase} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {EVENT_TYPES} from '../event_history.js';
import type {CommonUpdateOutcome, EventType} from '../event_history.js';
import {getKnownApps} from '../known_apps.js';

import {FilterCategory, getFilterCategoryForTarget} from './filter_bar.js';
import type {FilterSettings} from './filter_bar.js';
import {getCss} from './filter_dialog.css.js';
import {getHtml} from './filter_dialog.html.js';

export type FilterMenuState = FilterCategory|'closed'|'type';

export class FilterDialogElement extends CrLitElement {
  static get is() {
    return 'filter-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      type: {type: String},
      appSearch: {type: String},
      pendingAppSelections: {type: Object},
      pendingEventTypeSelections: {type: Object},
      pendingUpdateOutcomeSelections: {type: Object},
      pendingStartDateFilter: {type: Object},
      pendingEndDateFilter: {type: Object},
      filterSettings: {type: Object},
    };
  }

  accessor type: FilterMenuState = 'closed';
  accessor appSearch = '';
  accessor pendingAppSelections = new Set<string>();
  protected accessor pendingEventTypeSelections = new Set<EventType>();
  protected accessor pendingUpdateOutcomeSelections =
      new Set<CommonUpdateOutcome>();
  protected accessor pendingStartDateFilter: Date|null = null;
  protected accessor pendingEndDateFilter: Date|null = null;
  private accessor filterSettings: FilterSettings|null = null;
  protected displayedApps: string[] = [];
  protected readonly filterMenuItems:
      Array<{filterCategory: FilterCategory, label: string}> = [
        {
          filterCategory: FilterCategory.APP,
          label: loadTimeData.getString('app'),
        },
        {
          filterCategory: FilterCategory.EVENT,
          label: loadTimeData.getString('eventType'),
        },
        {
          filterCategory: FilterCategory.OUTCOME,
          label: loadTimeData.getString('updateOutcome'),
        },
        {
          filterCategory: FilterCategory.DATE,
          label: loadTimeData.getString('date'),
        },
      ];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('type') && this.filterSettings &&
        this.type !== 'type') {
      // Initialize pending state when entering a specific filter mode
      this.resetPendingState();
    }

    if (changedProperties.has('appSearch') ||
        changedProperties.has('pendingAppSelections')) {
      this.displayedApps = this.computeDisplayedApps();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('type') && this.type !== 'closed') {
      const focusTarget = this.shadowRoot.querySelector<HTMLElement>(
          '.filter-menu-input, .filter-menu-item, .filter-menu-date-inputs input, .filter-menu-footer cr-button');
      assert(focusTarget !== null);
      focusTarget.focus();
    }
  }

  private computeDisplayedApps(): string[] {
    const apps = getKnownApps();
    const predefinedApps = Array.from(apps.keys()).filter(appName => {
      return appName.toLowerCase().includes(this.appSearch.toLowerCase());
    });
    const customApps = Array.from(this.pendingAppSelections).filter(appName => {
      return !apps.has(appName) &&
          appName.toLowerCase().includes(this.appSearch.toLowerCase());
    });
    const appCandidates = new Set([...predefinedApps, ...customApps]);
    if (this.appSearch && !apps.has(this.appSearch) &&
        !this.pendingAppSelections.has(this.appSearch)) {
      appCandidates.add(this.appSearch);
    }
    return Array.from(appCandidates);
  }

  private resetPendingState() {
    if (!this.filterSettings) {
      return;
    }
    switch (this.type) {
      case FilterCategory.APP:
        this.pendingAppSelections =
            new Set(this.filterSettings.activeAppFilters);
        this.appSearch = '';
        break;
      case FilterCategory.EVENT:
        this.pendingEventTypeSelections =
            new Set(this.filterSettings.activeEventTypeFilters);
        break;
      case FilterCategory.OUTCOME:
        this.pendingUpdateOutcomeSelections =
            new Set(this.filterSettings.activeUpdateOutcomeFilters);
        break;
      case FilterCategory.DATE:
        this.pendingStartDateFilter = this.filterSettings.startDateFilter;
        this.pendingEndDateFilter = this.filterSettings.endDateFilter;
        break;
      case 'closed':
      case 'type':
        assertNotReached('No specific filter mode is active');
      default:
        assertNotReachedCase(this.type);
    }
  }

  get commonEventTypes(): EventType[] {
    return ['UPDATE', 'INSTALL', 'UNINSTALL'] as EventType[];
  }

  get otherEventTypes(): EventType[] {
    const common = this.commonEventTypes;
    return Object.values(EVENT_TYPES).filter(et => !common.includes(et));
  }

  protected get pendingStartDate(): number {
    return this.pendingStartDateFilter?.getTime() || NaN;
  }

  protected get pendingEndDate(): number {
    return this.pendingEndDateFilter?.getTime() || NaN;
  }

  protected onMenuKeydown(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      this.fire('close');
    } else if (e.key === 'Tab') {
      this.handleFocusTrap(e);
    }
  }

  private handleFocusTrap(e: KeyboardEvent) {
    const filterMenu = e.currentTarget as HTMLElement;
    const focusableElements: HTMLElement[] =
        Array.from(filterMenu.querySelectorAll(
            'input, cr-checkbox, cr-button, [tabindex="0"]'));

    if (focusableElements.length === 0) {
      return;
    }

    const firstFocusableElement = focusableElements[0];
    assert(firstFocusableElement !== undefined);
    const lastFocusableElement =
        focusableElements[focusableElements.length - 1];
    assert(lastFocusableElement !== undefined);
    const activeElement = this.shadowRoot.activeElement as HTMLElement;

    if (activeElement === lastFocusableElement) {
      firstFocusableElement.focus();
      e.preventDefault();
      return;
    }
    if (e.shiftKey && activeElement === firstFocusableElement) {
      lastFocusableElement.focus();
      e.preventDefault();
      return;
    }
  }

  protected onAppSearchInput(e: InputEvent) {
    this.appSearch = (e.target as HTMLInputElement).value;
  }

  protected onAppSearchKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      const input = e.target as HTMLInputElement;
      if (input.value) {
        this.pendingAppSelections.add(input.value);
        this.appSearch = '';
        this.requestUpdate();
      }
    }
  }

  protected onAppCheckboxCheckedChanged(e: Event) {
    const checkbox = e.target as HTMLInputElement;
    const appName = checkbox.dataset['appName']!;
    if (checkbox.checked) {
      this.pendingAppSelections.add(appName);
    } else {
      this.pendingAppSelections.delete(appName);
    }
    this.requestUpdate();
  }

  protected onAppApplyClick() {
    this.fire('app-filter-changed', this.pendingAppSelections);
  }

  protected onEventTypeCheckboxCheckedChanged(e: Event) {
    const checkbox = e.target as CrCheckboxElement;
    const eventType = checkbox.dataset['eventType'] as EventType;
    if (checkbox.checked) {
      this.pendingEventTypeSelections.add(eventType);
    } else {
      this.pendingEventTypeSelections.delete(eventType);
    }
    this.requestUpdate();
  }

  protected onEventTypeApplyClick() {
    this.fire('event-filter-changed', this.pendingEventTypeSelections);
  }

  protected onUpdateOutcomeCheckboxCheckedChanged(e: Event) {
    const checkbox = e.target as HTMLInputElement;
    const outcome = checkbox.dataset['outcome'] as CommonUpdateOutcome;
    if (checkbox.checked) {
      this.pendingUpdateOutcomeSelections.add(outcome);
    } else {
      this.pendingUpdateOutcomeSelections.delete(outcome);
    }
    this.requestUpdate();
  }

  protected onOutcomeApplyClick() {
    this.fire('outcome-filter-changed', this.pendingUpdateOutcomeSelections);
  }

  protected onStartTimeInput(e: Event) {
    const value = (e.target as HTMLInputElement).valueAsNumber;
    this.pendingStartDateFilter = Number.isNaN(value) ? null : new Date(value);
  }

  protected onEndTimeInput(e: Event) {
    const value = (e.target as HTMLInputElement).valueAsNumber;
    this.pendingEndDateFilter = Number.isNaN(value) ? null : new Date(value);
  }

  protected onCancelClick() {
    this.fire('close');
  }

  protected onDateApplyClick() {
    this.fire('date-filter-changed', {
      start: this.pendingStartDateFilter,
      end: this.pendingEndDateFilter,
    });
  }

  protected onTypeMenuKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      const target = e.target as HTMLElement;
      this.fire('type-selection-changed', getFilterCategoryForTarget(target));
    }
  }

  protected onTypeMenuClick(e: MouseEvent) {
    this.fire(
        'type-selection-changed',
        getFilterCategoryForTarget(e.target as HTMLElement));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'filter-dialog': FilterDialogElement;
  }
}

customElements.define(FilterDialogElement.is, FilterDialogElement);
