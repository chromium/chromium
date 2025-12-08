// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../icons.html.js';

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

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

export type FilterCategory = 'app'|'event'|'outcome'|'date';

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
      appSearch: {type: String},
      pendingAppSelections: {type: Object},
      pendingEventTypeSelections: {type: Object},
      pendingUpdateOutcomeSelections: {type: Object},
      pendingStartDateFilter: {type: Object},
      pendingEndDateFilter: {type: Object},
      focusReturnElement: {type: Object},
    };
  }

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
  accessor filterOrder: FilterCategory[] = [];
  accessor filterMenuState: 'closed'|'type'|'app'|'event'|'date'|'outcome' =
      'closed';
  accessor menuHost: 'chip'|'input' = 'input';
  accessor appSearch = '';
  accessor pendingAppSelections = new Set<string>();
  accessor pendingEventTypeSelections = new Set<EventType>();
  accessor pendingUpdateOutcomeSelections = new Set<CommonUpdateOutcome>();
  accessor pendingStartDateFilter: Date|null = null;
  accessor pendingEndDateFilter: Date|null = null;
  accessor focusReturnElement: HTMLElement|null = null;

  openFilterMenu() {
    if (this.menuHost === 'input' && this.filterMenuState !== 'closed') {
      this.closeFilterMenu();
      return;
    }
    this.focusReturnElement = this.shadowRoot.activeElement as HTMLElement;
    this.menuHost = 'input';
    this.filterMenuState = 'type';
    this.appSearch = '';
    this.pendingAppSelections = new Set<string>();
    this.pendingEventTypeSelections = new Set<EventType>();
    this.pendingUpdateOutcomeSelections = new Set<CommonUpdateOutcome>();
  }

  closeFilterMenu() {
    this.filterMenuState = 'closed';
    this.focusReturnElement?.focus();
    this.focusReturnElement = null;
  }

  override connectedCallback() {
    super.connectedCallback();
    document.addEventListener('click', this.handleClickOutside);
    if (this.filterOrder.length === 0) {
      const initialFilterOrder: FilterCategory[] = [];
      if (this.filterSettings.activeAppFilters.size > 0) {
        initialFilterOrder.push('app');
      }
      if (this.filterSettings.activeEventTypeFilters.size > 0) {
        initialFilterOrder.push('event');
      }
      if (this.filterSettings.activeUpdateOutcomeFilters.size > 0) {
        initialFilterOrder.push('outcome');
      }
      if (this.filterSettings.startDateFilter ||
          this.filterSettings.endDateFilter) {
        initialFilterOrder.push('date');
      }
      this.filterOrder = initialFilterOrder;
    }
  }
  override disconnectedCallback() {
    document.removeEventListener('click', this.handleClickOutside);
    super.disconnectedCallback();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    if (changedProperties.has('filterSettings')) {
      this.updateFilterOrder(
          'app',
          this.filterSettings.activeAppFilters.size > 0,
      );
      this.updateFilterOrder(
          'event',
          this.filterSettings.activeEventTypeFilters.size > 0,
      );
      this.updateFilterOrder(
          'outcome',
          this.filterSettings.activeUpdateOutcomeFilters.size > 0,
      );
      this.updateFilterOrder(
          'date',
          !!(this.filterSettings.startDateFilter ||
             this.filterSettings.endDateFilter),
      );
    }
  }

  override updated(changedProperties: PropertyValues) {
    if (changedProperties.has('filterMenuState') &&
        this.filterMenuState !== 'closed') {
      const focusTarget = this.renderRoot.querySelector<HTMLElement>(
          '.filter-menu-input, .filter-menu-item, .filter-menu-date-inputs input',
      );
      focusTarget?.focus();
    }
  }

  private readonly handleClickOutside = (e: MouseEvent) => {
    if (this.filterMenuState === 'closed') {
      return;
    }
    const path = e.composedPath() as Array<EventTarget|HTMLElement>;
    if (path.some(
            (el) => el ===
                this.shadowRoot.getElementById('filter-bar-input-container')!,
            )) {
      return;
    }
    if (path.some(
            (el) => el instanceof HTMLElement &&
                (el.classList.contains('chip') ||
                 el.classList.contains('filter-menu')),
            )) {
      return;
    }
    this.closeFilterMenu();
  };

  handleAppSearchKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      const input = e.target as HTMLInputElement;
      if (input.value) {
        this.pendingAppSelections.add(input.value);
        this.appSearch = '';
        this.requestUpdate();
      }
    }
  }

  onAppCheckboxChange(e: Event, appName: string) {
    const checkbox = e.target as HTMLInputElement;
    if (checkbox.checked) {
      this.pendingAppSelections.add(appName);
    } else {
      this.pendingAppSelections.delete(appName);
    }
    this.requestUpdate();
  }

  onEventTypeCheckboxChange(e: Event, eventType: EventType) {
    const checkbox = e.target as CrCheckboxElement;
    if (checkbox.checked) {
      this.pendingEventTypeSelections.add(eventType);
    } else {
      this.pendingEventTypeSelections.delete(eventType);
    }
    this.requestUpdate();
  }

  onUpdateOutcomeCheckboxChange(e: Event, outcome: CommonUpdateOutcome) {
    const checkbox = e.target as HTMLInputElement;
    if (checkbox.checked) {
      this.pendingUpdateOutcomeSelections.add(outcome);
    } else {
      this.pendingUpdateOutcomeSelections.delete(outcome);
    }
    this.requestUpdate();
  }

  updateFilterOrder(category: FilterCategory, active: boolean) {
    const index = this.filterOrder.indexOf(category);
    if (active && index === -1) {
      this.filterOrder = [...this.filterOrder, category];
    } else if (!active && index !== -1) {
      this.filterOrder = this.filterOrder.filter((f) => f !== category);
    }
  }

  applyAppFilters() {
    this.updateFilterOrder('app', this.pendingAppSelections.size > 0);
    this.filterSettings.activeAppFilters = new Set(this.pendingAppSelections);
    this.closeFilterMenu();
    this.dispatchFiltersChanged();
  }

  applyEventTypeFilters() {
    this.updateFilterOrder('event', this.pendingEventTypeSelections.size > 0);
    this.filterSettings.activeEventTypeFilters = new Set(
        this.pendingEventTypeSelections,
    );
    this.closeFilterMenu();
    this.dispatchFiltersChanged();
  }

  applyUpdateOutcomeFilters() {
    this.updateFilterOrder(
        'outcome',
        this.pendingUpdateOutcomeSelections.size > 0,
    );
    this.filterSettings.activeUpdateOutcomeFilters = new Set(
        this.pendingUpdateOutcomeSelections,
    );
    this.closeFilterMenu();
    this.dispatchFiltersChanged();
  }

  applyDateFilters() {
    this.updateFilterOrder(
        'date',
        !!(this.pendingStartDateFilter || this.pendingEndDateFilter),
    );
    this.filterSettings.startDateFilter = this.pendingStartDateFilter;
    this.filterSettings.endDateFilter = this.pendingEndDateFilter;
    this.closeFilterMenu();
    this.dispatchFiltersChanged();
  }

  editDateFilter() {
    this.focusReturnElement = this.shadowRoot.activeElement as HTMLElement;
    this.menuHost = 'chip';
    this.filterMenuState = 'date';
    this.pendingStartDateFilter = this.filterSettings.startDateFilter;
    this.pendingEndDateFilter = this.filterSettings.endDateFilter;
  }

  editAppFilter() {
    this.focusReturnElement = this.shadowRoot.activeElement as HTMLElement;
    this.menuHost = 'chip';
    this.filterMenuState = 'app';
    this.pendingAppSelections = new Set(this.filterSettings.activeAppFilters);
    this.appSearch = '';
  }

  editEventTypeFilter() {
    this.focusReturnElement = this.shadowRoot.activeElement as HTMLElement;
    this.menuHost = 'chip';
    this.filterMenuState = 'event';
    this.pendingEventTypeSelections = new Set(
        this.filterSettings.activeEventTypeFilters,
    );
  }

  editUpdateOutcomeFilter() {
    this.focusReturnElement = this.shadowRoot.activeElement as HTMLElement;
    this.menuHost = 'chip';
    this.filterMenuState = 'outcome';
    this.pendingUpdateOutcomeSelections = new Set(
        this.filterSettings.activeUpdateOutcomeFilters,
    );
  }

  handleChipKeydown(e: KeyboardEvent, editFilter: () => void) {
    if ((e.key === 'Enter' || e.key === ' ') &&
        (e.target as HTMLElement).tagName !== 'BUTTON') {
      editFilter();
    }
  }

  getDateFilterString(): string {
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

  dispatchFiltersChanged() {
    this.dispatchEvent(
        new CustomEvent<FilterSettings>('filters-changed', {
          detail: this.filterSettings,
          bubbles: true,
          composed: true,
        }),
    );
    this.requestUpdate();
  }

  handleMenuKeydown(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      this.closeFilterMenu();
    } else if (e.key === 'Tab') {
      this.handleFocusTrap(e);
    }
  }

  private handleFocusTrap(e: KeyboardEvent) {
    const filterMenu = e.currentTarget as HTMLElement;
    const focusableElements: HTMLElement[] = Array.from(
        filterMenu.querySelectorAll(
            'input, cr-checkbox, cr-button, [tabindex="0"]'),
    );

    if (focusableElements.length === 0) {
      return;
    }

    const firstFocusableElement = focusableElements[0];
    const lastFocusableElement =
        focusableElements[focusableElements.length - 1];
    const activeElement = this.shadowRoot.activeElement as HTMLElement;

    if (e.shiftKey) {
      if (activeElement === firstFocusableElement) {
        lastFocusableElement?.focus();
        e.preventDefault();
      }
    } else if (activeElement === lastFocusableElement) {
      firstFocusableElement?.focus();
      e.preventDefault();
    }
  }

  clearAllFilters() {
    this.filterSettings.activeAppFilters.clear();
    this.filterSettings.activeEventTypeFilters.clear();
    this.filterSettings.activeUpdateOutcomeFilters.clear();
    this.filterSettings.startDateFilter = null;
    this.filterSettings.endDateFilter = null;
    this.filterOrder = [];
    this.dispatchFiltersChanged();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'filter-bar': FilterBarElement;
  }
}

customElements.define(FilterBarElement.is, FilterBarElement);
