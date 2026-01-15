// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './filter_dialog/filter_dialog.js';
import './filter_dialog/app_dialog.js';
import './filter_dialog/event_dialog.js';
import './filter_dialog/outcome_dialog.js';
import './filter_dialog/scope_dialog.js';
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
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {localizeEventType, localizeScope, localizeUpdateOutcome} from '../event_history.js';
import type {CommonUpdateOutcome, EventType, Scope} from '../event_history.js';
import {loadTimeData} from '../i18n_setup.js';
import {formatDateDigits} from '../tools.js';

import {getCss} from './filter_bar.css.js';
import {getHtml} from './filter_bar.html.js';
import {createDefaultFilterSettings} from './filter_settings.js';
import type {FilterSettings} from './filter_settings.js';

export enum FilterCategory {
  APP = 'app',
  EVENT = 'event',
  OUTCOME = 'outcome',
  DATE = 'date',
  SCOPE = 'scope',
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

  accessor filterSettings: FilterSettings = createDefaultFilterSettings();
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
      if (this.filterSettings.apps.size > 0) {
        initialFilterOrder.push(FilterCategory.APP);
      }
      if (this.filterSettings.eventTypes.size > 0) {
        initialFilterOrder.push(FilterCategory.EVENT);
      }
      if (this.filterSettings.updateOutcomes.size > 0) {
        initialFilterOrder.push(FilterCategory.OUTCOME);
      }
      if (this.filterSettings.scopes.size > 0) {
        initialFilterOrder.push(FilterCategory.SCOPE);
      }
      if (this.filterSettings.startDate || this.filterSettings.endDate) {
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
          FilterCategory.APP, this.filterSettings.apps.size > 0);
      this.updateFilterOrder(
          FilterCategory.EVENT, this.filterSettings.eventTypes.size > 0);
      this.updateFilterOrder(
          FilterCategory.OUTCOME, this.filterSettings.updateOutcomes.size > 0);
      this.updateFilterOrder(
          FilterCategory.SCOPE, this.filterSettings.scopes.size > 0);
      this.updateFilterOrder(
          FilterCategory.DATE,
          !!(this.filterSettings.startDate || this.filterSettings.endDate));
    }
  }


  protected async onRemoveFilterClick(e: Event) {
    const category = getFilterCategoryForTarget(e.target as HTMLElement);
    this.updateFilterOrder(category, false);
    switch (category) {
      case FilterCategory.APP:
        this.filterSettings.apps.clear();
        break;
      case FilterCategory.EVENT:
        this.filterSettings.eventTypes.clear();
        break;
      case FilterCategory.OUTCOME:
        this.filterSettings.updateOutcomes.clear();
        break;
      case FilterCategory.SCOPE:
        this.filterSettings.scopes.clear();
        break;
      case FilterCategory.DATE:
        this.filterSettings.startDate = null;
        this.filterSettings.endDate = null;
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
            'filterChipApp', Array.from(this.filterSettings.apps).join(', '));
      case FilterCategory.EVENT:
        return loadTimeData.getStringF(
            'filterChipEventType',
            Array.from(this.filterSettings.eventTypes)
                .map(localizeEventType)
                .join(', '));
      case FilterCategory.OUTCOME:
        return loadTimeData.getStringF(
            'filterChipUpdateOutcome',
            Array.from(this.filterSettings.updateOutcomes)
                .map(localizeUpdateOutcome)
                .join(', '));
      case FilterCategory.SCOPE:
        return loadTimeData.getStringF(
            'filterChipUpdaterScope',
            Array.from(this.filterSettings.scopes)
                .map(localizeScope)
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
    this.filterSettings.apps = new Set(e.detail);
    this.closeFilterMenu();
    await this.onFiltersChanged();
  }

  protected async onEventTypeFilterChange(e: CustomEvent<Set<EventType>>) {
    this.updateFilterOrder(FilterCategory.EVENT, e.detail.size > 0);
    this.filterSettings.eventTypes = new Set(e.detail);
    this.closeFilterMenu();
    await this.onFiltersChanged();
  }

  protected async onUpdateOutcomeFilterChange(
      e: CustomEvent<Set<CommonUpdateOutcome>>) {
    this.updateFilterOrder(FilterCategory.OUTCOME, e.detail.size > 0);
    this.filterSettings.updateOutcomes = new Set(e.detail);
    this.closeFilterMenu();
    await this.onFiltersChanged();
  }

  protected async onScopeFilterChange(e: CustomEvent<Set<Scope>>) {
    this.updateFilterOrder(FilterCategory.SCOPE, e.detail.size > 0);
    this.filterSettings.scopes = new Set(e.detail);
    this.closeFilterMenu();
    await this.onFiltersChanged();
  }

  protected async onDateFilterChange(
      e: CustomEvent<{start: Date | null, end: Date|null}>) {
    this.updateFilterOrder(
        FilterCategory.DATE, !!(e.detail.start || e.detail.end));
    this.filterSettings.startDate = e.detail.start;
    this.filterSettings.endDate = e.detail.end;
    this.closeFilterMenu();
    await this.onFiltersChanged();
  }

  protected getDateFilterString(): string {
    const format = (d: Date) => formatDateDigits(d);
    const start = this.filterSettings.startDate ?
        format(this.filterSettings.startDate) :
        undefined;
    const end = this.filterSettings.endDate ?
        format(this.filterSettings.endDate) :
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
    this.fire('filters-changed');
    this.requestUpdate();
  }

  protected async onClearFiltersClick() {
    this.filterSettings.apps.clear();
    this.filterSettings.eventTypes.clear();
    this.filterSettings.updateOutcomes.clear();
    this.filterSettings.scopes.clear();
    this.filterSettings.startDate = null;
    this.filterSettings.endDate = null;
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
