// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './event_list_item.js';
import './filter_bar.js';
import './raw_event_details.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {assert} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

import {deduplicateEvents, mergeEvents, parseEvents, UpdaterProcessMap} from '../event_history.js';
import type {HistoryEvent, MergedHistoryEvent} from '../event_history.js';
import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './event_list.css.js';
import {getHtml} from './event_list.html.js';
import type {EventListItemElement} from './event_list_item.js';
import {applyFilterSettings, createDefaultFilterSettings} from './filter_settings.js';
import type {FilterSettings} from './filter_settings.js';

/**
 * Maps a set of events to EventEntry objects, which have all of the necessary
 * information to render an event-list-item. All of the provided events must
 * have dates (via processMap).
 */
function getEventEntries(
    processMap: UpdaterProcessMap|undefined,
    events: Array<HistoryEvent|MergedHistoryEvent>): EventEntry[] {
  if (processMap === undefined) {
    return [];
  }
  return events.map((event, index) => {
    const eventDate = processMap.eventDate(event);
    assert(eventDate !== undefined);
    const nextEvent = events[index - 1];
    const nextEventDate =
        nextEvent ? processMap.eventDate(nextEvent) : undefined;
    return {
      event,
      shouldShowBreak: index > 0 && nextEventDate !== undefined &&
          nextEventDate.getTime() - eventDate.getTime() > 1000 * 60 * 60,
      eventDate,
      formattedEventDate: eventDate.toLocaleString(),
      formattedRelativeEventDate: getRelativeDate(eventDate),
    };
  });
}

function getRelativeDate(date: Date): string {
  const now = new Date();
  const diffInSeconds = (now.getTime() - date.getTime()) / 1000;
  const rtf = new Intl.RelativeTimeFormat();

  if (diffInSeconds < 60) {
    return rtf.format(-Math.floor(diffInSeconds), 'second');
  }
  const diffInMinutes = diffInSeconds / 60;
  if (diffInMinutes < 60) {
    return rtf.format(-Math.floor(diffInMinutes), 'minute');
  }
  const diffInHours = diffInMinutes / 60;
  if (diffInHours < 24) {
    return rtf.format(-Math.floor(diffInHours), 'hour');
  }
  const diffInDays = diffInHours / 24;
  return rtf.format(-Math.floor(diffInDays), 'day');
}

interface EventEntry {
  event: HistoryEvent|MergedHistoryEvent;
  // Whether a list break should be displayed before the entry.
  shouldShowBreak: boolean;
  eventDate: Date;
  formattedEventDate: string;
  formattedRelativeEventDate: string;
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
    };
  }

  accessor messages: Array<Record<string, unknown>> = [];
  accessor filterSettings: FilterSettings = createDefaultFilterSettings();
  protected accessor eventsWithoutDatesLabel: string = '';
  protected accessor eventsWithParseErrorsLabel: string = '';
  protected accessor expandAllButtonLabel: string =
      loadTimeData.getString('expandAll');
  protected accessor events: EventEntry[] = [];

  protected processMap: UpdaterProcessMap|undefined = undefined;
  protected eventsWithParseErrors: Array<Record<string, unknown>> = [];
  protected eventsWithoutDates: Array<HistoryEvent|MergedHistoryEvent> = [];
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
      this.eventsWithParseErrors = invalid;
      this.eventsWithoutDates = unsortedEventsWithoutDates;
      this.sortedEventsWithDates = sortedEventsWithDates;

      const pluralStringProxy = PluralStringProxyImpl.getInstance();

      pluralStringProxy
          .getPluralString('undatedEvents', this.eventsWithoutDates.length)
          .then(label => this.eventsWithoutDatesLabel = label);
      pluralStringProxy
          .getPluralString(
              'parseErrorEvents', this.eventsWithParseErrors.length)
          .then(label => this.eventsWithParseErrorsLabel = label);
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
    this.events = getEventEntries(this.processMap, filteredEvents);
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
}

declare global {
  interface HTMLElementTagNameMap {
    'event-list': EventListElement;
  }
}

customElements.define(EventListElement.is, EventListElement);
