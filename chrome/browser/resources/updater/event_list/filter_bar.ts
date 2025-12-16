// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './filter_dialog/filter_dialog.js';
import './filter_dialog/app_dialog.js';
import './filter_dialog/event_dialog.js';
import './filter_dialog/outcome_dialog.js';
import './filter_dialog/date_dialog.js';
import './filter_dialog/type_dialog.js';
import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../icons.html.js';

import {assertNotReachedCase} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {localizeEventType, localizeUpdateOutcome} from '../event_history.js';
import type {CommonUpdateOutcome, EventType} from '../event_history.js';

import {getCss} from './filter_bar.css.js';
import {getHtml} from './filter_bar.html.js';

const DATE_FORMATTER = new Intl.DateTimeFormat(undefined, {
  year: 'numeric',
  month: '2-digit',
  day: '2-digit',
  hour: '2-digit',
  minute: '2-digit',
});

/**
 * The filter settings for the event list.
 */
export interface FilterSettings {
  activeAppFilters: Set<string>;
  activeEventTypeFilters: Set<EventType>;
  activeUpdateOutcomeFilters: Set<CommonUpdateOutcome>;
  startDateFilter: Date|null;
  endDateFilter: Date|null;
}

export enum FilterCategory {
  APP = 'app',
  EVENT = 'event',
  OUTCOME = 'outcome',
  DATE = 'date',
}

export type FilterMenuState = FilterCategory|'closed'|'type';

/**
 * Returns the filter category associated with an element via the
 * data-filter-category property.
 */
export function getFilterCategoryForTarget(target: HTMLElement):
    FilterCategory {
  return target.dataset['filterCategory'] as FilterCategory;
}

export interface FilterBarElement {
  $: {
    filterBarInputContainer: HTMLElement,
  };
}

export class FilterBarElement extends CrLitElement {
  static get is() {
    return 'filter-bar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      filterSettings: {type: Object},
      filterOrder: {type: Array},
      filterMenuState: {type: String},
      menuHost: {type: String},
      focusReturnElement: {type: Object},
    };
  }

  private eventTracker: EventTracker = new EventTracker();

  accessor filterSettings: FilterSettings = {
    activeAppFilters: (() => {
      const defaultApps = loadTimeData.getString('defaultAppFilters');
      return new Set(defaultApps === '' ? [] : defaultApps.split(','));
    })(),
    activeEventTypeFilters: new Set<EventType>([
      'INSTALL',
      'UPDATE',
      'UNINSTALL',
    ]),
    activeUpdateOutcomeFilters:
        new Set<CommonUpdateOutcome>(['UPDATED', 'UPDATE_ERROR']),
    startDateFilter: null,
    endDateFilter: null,
  };
  protected accessor filterOrder: FilterCategory[] = [];
  protected accessor filterMenuState: FilterMenuState = 'closed';
  protected accessor menuHost: 'chip'|'input' = 'input';

  private accessor focusReturnElement: HTMLElement|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker.add(document, 'click', (e: MouseEvent) => {
      if (this.filterMenuState === 'closed') {
        return;
      }
      const path = e.composedPath() as Array<EventTarget|HTMLElement>;
      if (path.some(el => el === this.$.filterBarInputContainer)) {
        return;
      }
      if (path.some(el => {
            return el instanceof HTMLElement &&
                (el.classList.contains('chip') ||
                 el.tagName === 'FILTER-DIALOG');
          })) {
        return;
      }
      this.closeFilterMenu();
    });
    if (this.filterOrder.length === 0) {
      const initialFilterOrder: FilterCategory[] = [];
      if (this.filterSettings.activeAppFilters.size > 0) {
        initialFilterOrder.push(FilterCategory.APP);
      }
      if (this.filterSettings.activeEventTypeFilters.size > 0) {
        initialFilterOrder.push(FilterCategory.DATE);
      }
      if (this.filterSettings.activeUpdateOutcomeFilters.size > 0) {
        initialFilterOrder.push(FilterCategory.EVENT);
      }
      if (this.filterSettings.startDateFilter ||
          this.filterSettings.endDateFilter) {
        initialFilterOrder.push(FilterCategory.DATE);
      }
      this.filterOrder = initialFilterOrder;
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('filterSettings')) {
      this.updateFilterOrder(
          FilterCategory.APP, this.filterSettings.activeAppFilters.size > 0);
      this.updateFilterOrder(
          FilterCategory.EVENT,
          this.filterSettings.activeEventTypeFilters.size > 0);
      this.updateFilterOrder(
          FilterCategory.OUTCOME,
          this.filterSettings.activeUpdateOutcomeFilters.size > 0);
      this.updateFilterOrder(
          FilterCategory.DATE,
          !!(this.filterSettings.startDateFilter ||
             this.filterSettings.endDateFilter));
    }
  }


  protected async onRemoveFilterClick(e: Event) {
    const category = getFilterCategoryForTarget(e.target as HTMLElement);
    this.updateFilterOrder(category, false);
    switch (category) {
      case FilterCategory.APP:
        this.filterSettings.activeAppFilters.clear();
        break;
      case FilterCategory.EVENT:
        this.filterSettings.activeEventTypeFilters.clear();
        break;
      case FilterCategory.OUTCOME:
        this.filterSettings.activeUpdateOutcomeFilters.clear();
        break;
      case FilterCategory.DATE:
        this.filterSettings.startDateFilter = null;
        this.filterSettings.endDateFilter = null;
        break;
      default:
        assertNotReachedCase(category);
    }
    await this.onFiltersChanged();
  }

  protected onChipClick(e: MouseEvent) {
    // Prevent opening the menu if the close button was clicked
    const target = e.target as HTMLElement;
    if (target.tagName !== 'CR-ICON-BUTTON') {
      this.editFilter(getFilterCategoryForTarget(target));
    }
  }

  protected onChipKeydown(e: KeyboardEvent) {
    const target = e.target as HTMLElement;
    if ((e.key === 'Enter' || e.key === ' ') && target.tagName !== 'BUTTON') {
      this.editFilter(getFilterCategoryForTarget(target));
    }
  }


  protected getFilterLabel(category: FilterCategory): string {
    switch (category) {
      case FilterCategory.APP:
        return loadTimeData.getStringF(
            'filterChipApp',
            Array.from(this.filterSettings.activeAppFilters).join(', '));
      case FilterCategory.EVENT:
        return loadTimeData.getStringF(
            'filterChipEventType',
            Array.from(this.filterSettings.activeEventTypeFilters)
                .map(localizeEventType)
                .join(', '));
      case FilterCategory.OUTCOME:
        return loadTimeData.getStringF(
            'filterChipUpdateOutcome',
            Array.from(this.filterSettings.activeUpdateOutcomeFilters)
                .map(localizeUpdateOutcome)
                .join(', '));
      case FilterCategory.DATE:
        return loadTimeData.getStringF(
            'filterChipDate', this.getDateFilterString());
      default:
        assertNotReachedCase(category);
    }
  }

  protected editFilter(category: FilterCategory) {
    this.focusReturnElement = this.shadowRoot.activeElement as HTMLElement;
    this.menuHost = 'chip';
    this.filterMenuState = category;
  }

  protected closeFilterMenu() {
    this.filterMenuState = 'closed';
    this.focusReturnElement?.focus();
    this.focusReturnElement = null;
  }

  protected updateFilterOrder(category: FilterCategory, active: boolean) {
    const index = this.filterOrder.indexOf(category);
    if (active && index === -1) {
      this.filterOrder = [...this.filterOrder, category];
    } else if (!active && index !== -1) {
      this.filterOrder = this.filterOrder.filter(f => f !== category);
    }
  }

  protected onTypeSelectionChanged(e: CustomEvent<FilterCategory>) {
    this.filterMenuState = e.detail;
  }

  protected async onAppFilterChange(e: CustomEvent<Set<string>>) {
    this.updateFilterOrder(FilterCategory.APP, e.detail.size > 0);
    this.filterSettings.activeAppFilters = new Set(e.detail);
    this.closeFilterMenu();
    await this.onFiltersChanged();
  }

  protected async onEventTypeFilterChange(e: CustomEvent<Set<EventType>>) {
    this.updateFilterOrder(FilterCategory.EVENT, e.detail.size > 0);
    this.filterSettings.activeEventTypeFilters = new Set(e.detail);
    this.closeFilterMenu();
    await this.onFiltersChanged();
  }

  protected async onUpdateOutcomeFilterChange(
      e: CustomEvent<Set<CommonUpdateOutcome>>) {
    this.updateFilterOrder(FilterCategory.OUTCOME, e.detail.size > 0);
    this.filterSettings.activeUpdateOutcomeFilters = new Set(e.detail);
    this.closeFilterMenu();
    await this.onFiltersChanged();
  }

  protected async onDateFilterChange(
      e: CustomEvent<{start: Date | null, end: Date|null}>) {
    this.updateFilterOrder(
        FilterCategory.DATE, !!(e.detail.start || e.detail.end));
    this.filterSettings.startDateFilter = e.detail.start;
    this.filterSettings.endDateFilter = e.detail.end;
    this.closeFilterMenu();
    await this.onFiltersChanged();
  }

  protected getDateFilterString(): string {
    const format = (d: Date) => DATE_FORMATTER.format(d);
    const start = this.filterSettings.startDateFilter ?
        format(this.filterSettings.startDateFilter) :
        undefined;
    const end = this.filterSettings.endDateFilter ?
        format(this.filterSettings.endDateFilter) :
        undefined;
    if (start && end) {
      return loadTimeData.getStringF('dateFilterRange', start, end);
    }
    if (start) {
      return loadTimeData.getStringF('dateFilterAfter', start);
    }
    if (end) {
      return loadTimeData.getStringF('dateFilterBefore', end);
    }
    return '';
  }

  private async onFiltersChanged() {
    await this.updateComplete;
    this.fire('filters-changed', this.filterSettings);
    this.requestUpdate();
  }

  protected async onClearFiltersClick() {
    this.filterSettings.activeAppFilters.clear();
    this.filterSettings.activeEventTypeFilters.clear();
    this.filterSettings.activeUpdateOutcomeFilters.clear();
    this.filterSettings.startDateFilter = null;
    this.filterSettings.endDateFilter = null;
    this.filterOrder = [];
    await this.onFiltersChanged();
  }

  protected isEditing(): boolean {
    return this.filterMenuState !== 'closed';
  }

  protected isEditingViaInput(category: FilterCategory): boolean {
    return this.filterMenuState === category && this.menuHost === 'input';
  }

  protected getDialogAnchor(): HTMLElement|null {
    if (this.menuHost === 'input') {
      return this.shadowRoot.querySelector('#add-filter-button');
    }
    if (this.filterMenuState !== 'closed') {
      return this.shadowRoot.querySelector(
          `.chip[data-filter-category="${this.filterMenuState}"]`);
    }
    return null;
  }

  protected onAddFilterClick() {
    if (this.menuHost === 'input' && this.filterMenuState !== 'closed') {
      this.closeFilterMenu();
      return;
    }
    this.focusReturnElement = this.shadowRoot.activeElement as HTMLElement;
    this.menuHost = 'input';
    this.filterMenuState = 'type';
  }

  protected onFilterDialogClose() {
    this.closeFilterMenu();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'filter-bar': FilterBarElement;
  }
}

customElements.define(FilterBarElement.is, FilterBarElement);
