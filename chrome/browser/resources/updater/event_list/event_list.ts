// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './event_list_item.js';
import './filter_bar.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_infinite_list/cr_infinite_list.js';

import {assert} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

import {deduplicateEvents, mergeEvents, parseEvents, UpdaterProcessMap} from '../event_history.js';
import type {HistoryEvent, MergedHistoryEvent, PolicySet} from '../event_history.js';
import {loadTimeData} from '../i18n_setup.js';
import {formatDateShort, formatRelativeDate} from '../tools.js';

import {getCss} from './event_list.css.js';
import {getHtml} from './event_list.html.js';
import type {EventListItemElement} from './event_list_item.js';
import {applyFilterSettings, createDefaultFilterSettings} from './filter_settings.js';
import type {FilterSettings} from './filter_settings.js';

/**
 * Returns the effective policy set for an event if one exists and should be
 * presented.
 */
function getEffectivePolicySet(
    processMap: UpdaterProcessMap, event: HistoryEvent|MergedHistoryEvent,
    allEvents: Array<HistoryEvent|MergedHistoryEvent>): PolicySet|undefined {
  return event.eventType === 'UPDATE' || event.eventType === 'INSTALL' ?
      processMap.effectivePolicySet(event, allEvents) :
      undefined;
}

/**
 * Maps a set of events to EventEntry objects, which have all of the necessary
 * information to render an event-list-item. All of the provided events must
 * have dates (via processMap).
 */
function getEventEntries(
    processMap: UpdaterProcessMap|undefined,
    filteredEvents: Array<HistoryEvent|MergedHistoryEvent>,
    allEvents: Array<HistoryEvent|MergedHistoryEvent>): EventEntry[] {
  if (processMap === undefined) {
    return [];
  }
  return filteredEvents.map(event => {
    const eventDate = processMap.eventDate(event);
    assert(eventDate !== undefined);
    return {
      event,
      eventDate,
      formattedEventDate: formatDateShort(eventDate),
      formattedRelativeEventDate: formatRelativeDate(eventDate),
      policies: getEffectivePolicySet(processMap, event, allEvents),
    };
  });
}

export interface EventEntry {
  event: HistoryEvent|MergedHistoryEvent;
  eventDate: Date;
  formattedEventDate: string;
  formattedRelativeEventDate: string;
  policies: PolicySet|undefined;
}

export class EventListElement extends CrLitElement {
  static get is() {
    return 'event-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      messages: {type: Array},
      filterSettings: {type: Object},
      eventsWithoutDatesLabel: {type: String},
      eventsWithParseErrorsLabel: {type: String},
      expandAllButtonLabel: {type: String},
      events: {type: Array},
      scrollTarget: {type: Object},
    };
  }

  accessor messages: Array<Record<string, unknown>> = [];
  accessor filterSettings: FilterSettings = createDefaultFilterSettings();
  protected accessor eventsWithoutDatesLabel: string = '';
  protected accessor eventsWithParseErrorsLabel: string = '';
  protected accessor expandAllButtonLabel: string =
      loadTimeData.getString('expandAll');
  protected accessor events: EventEntry[] = [];
  protected accessor scrollTarget: HTMLElement = document.documentElement;

  protected processMap: UpdaterProcessMap|undefined = undefined;
  protected sortedEventsWithDates: Array<HistoryEvent|MergedHistoryEvent> = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('messages')) {
      const {valid, invalid} = parseEvents(this.messages);
      const deduplicated = deduplicateEvents(valid);
      const {paired, unpaired} = mergeEvents(deduplicated);
      const processMap = new UpdaterProcessMap(paired);
      const {sortedEventsWithDates, unsortedEventsWithoutDates} =
          processMap.sortEventsByDate(unpaired, paired);

      this.processMap = processMap;
      this.sortedEventsWithDates = sortedEventsWithDates;

      const pluralStringProxy = PluralStringProxyImpl.getInstance();

      if (unsortedEventsWithoutDates.length === 0) {
        this.eventsWithoutDatesLabel = '';
      } else {
        pluralStringProxy
            .getPluralString('undatedEvents', unsortedEventsWithoutDates.length)
            .then(label => this.eventsWithoutDatesLabel = label);
      }

      if (invalid.length === 0) {
        this.eventsWithParseErrorsLabel = '';
      } else {
        pluralStringProxy.getPluralString('parseErrorEvents', invalid.length)
            .then(label => this.eventsWithParseErrorsLabel = label);
      }
    }

    if (changedProperties.has('messages') ||
        changedProperties.has('filterSettings')) {
      this.updateEventEntries();
    }
  }

  private expandAll() {
    this.eventListItems.forEach((item) => {
      item.expand();
    });
  }

  private collapseAll() {
    this.eventListItems.forEach((item) => {
      item.collapse();
    });
  }

  protected get anyExpanded(): boolean {
    return this.eventListItems.some(item => item.expanded);
  }

  protected get eventListItems(): EventListItemElement[] {
    return Array.from(this.shadowRoot.querySelectorAll('event-list-item'));
  }

  updateEventEntries() {
    const filteredEvents = applyFilterSettings(
        this.processMap, this.sortedEventsWithDates, this.filterSettings);
    this.events = getEventEntries(
        this.processMap, filteredEvents, this.sortedEventsWithDates);
  }

  protected onFiltersChanged() {
    // Subfields of the filter settings have changed, however this does not
    // trigger a new render cycle with an updated filterSettings property.
    // Compute the new `events` array explicitly, which will trigger a new
    // render cycle.
    this.updateEventEntries();
  }

  protected onExpandCollapseAllClick() {
    if (this.anyExpanded) {
      this.collapseAll();
    } else {
      this.expandAll();
    }
  }

  protected onEventItemExpandedChanged() {
    this.expandAllButtonLabel =
        loadTimeData.getString(this.anyExpanded ? 'collapseAll' : 'expandAll');
  }

  protected get numDisplayedEventsLabel(): string {
    return loadTimeData.getStringF(
        'displayedEventsCount', this.events.length,
        this.sortedEventsWithDates.length);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'event-list': EventListElement;
  }
}

customElements.define(EventListElement.is, EventListElement);
