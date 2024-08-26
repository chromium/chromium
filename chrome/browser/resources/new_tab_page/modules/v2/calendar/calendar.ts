// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './calendar_event.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CalendarEvent} from '../../../calendar_data.mojom-webui.js';
import {WindowProxy} from '../../../window_proxy.js';

import {getCss} from './calendar.css.js';
import {getHtml} from './calendar.html.js';
import {CalendarAction, recordCalendarAction, toJsTimestamp} from './common.js';

export interface CalendarElement {
  $: {
    seeMore: HTMLElement,
  };
}

/**
 * The calendar element for displaying the user's list of events. .
 */
export class CalendarElement extends CrLitElement {
  static get is() {
    return 'ntp-calendar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      calendarLink: {type: String},
      events: {type: Object},
      moduleName: {type: String},
      doubleBookedIndices_: {type: Object},
      expandedEventIndex_: {type: Number},
    };
  }

  calendarLink: string;
  events: CalendarEvent[] = [];
  moduleName: string;

  private doubleBookedIndices_: number[] = [];
  private expandedEventIndex_: number;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('events')) {
      chrome.metricsPrivate.recordSmallCount(
          `NewTabPage.${this.moduleName}.ShownEvents`, this.events.length);
      this.expandedEventIndex_ = this.computeExpandedEventIndex_();
      if (this.expandedEventIndex_ !== -1) {
        this.sortEvents_();
        this.doubleBookedIndices_ = this.computeDoubleBookedIndices_();
      }
    }
  }

  private computeDoubleBookedIndices_(): number[] {
    const results: number[] = [];
    for (let i = this.expandedEventIndex_ + 1; i < this.events.length; i++) {
      if (this.events[i].startTime.internalValue ===
          this.events[this.expandedEventIndex_].startTime.internalValue) {
        results.push(i);
      } else {
        break;
      }
    }
    return results;
  }

  private compareEventPriority_(
      eventAIndex: number, eventBIndex: number, soon: number): number {
    const eventA = this.events[eventAIndex];
    const eventB = this.events[eventBIndex];
    const eventAStartTime = toJsTimestamp(eventA.startTime);
    const eventBStartTime = toJsTimestamp(eventB.startTime);
    const eventAInProgress = eventAStartTime <= soon;
    const eventBInProgress = eventBStartTime <= soon;

    // Check if either is in progress or starting soon and the other isn't.
    if (eventAInProgress !== eventBInProgress) {
      return +eventBInProgress - +eventAInProgress;
    }

    const eventAEndsSoon = toJsTimestamp(eventA.endTime) <= soon;
    const eventBEndsSoon = toJsTimestamp(eventB.endTime) <= soon;
    // Check if either is ending soon and the other isn't.
    if (eventAEndsSoon !== eventBEndsSoon) {
      return +eventAEndsSoon - +eventBEndsSoon;
    }

    // Check for startTime. But only prioritize by this if both are not in
    // progress or soon.
    if (!eventAInProgress && !eventBInProgress &&
        eventAStartTime !== eventBStartTime) {
      return eventAStartTime - eventBStartTime;
    }

    // Check if event is accepted.
    if (eventA.isAccepted !== eventB.isAccepted) {
      return +eventB.isAccepted - +eventA.isAccepted;
    }

    // Check if there are other attendees.
    if (eventA.hasOtherAttendee !== eventB.hasOtherAttendee) {
      return +eventB.hasOtherAttendee - +eventA.hasOtherAttendee;
    }

    // If there is still a tie, return in the order they will show in the
    // list, which is what is returned by the API (startTime with
    // unspecified tie breaker).
    return eventAIndex - eventBIndex;
  }

  private computeExpandedEventIndex_(): number {
    const now = WindowProxy.getInstance().now();

    // Find the indices of all meetings that are not over.
    let expandableEventIndices: number[] = this.events.map((_, i) => i);
    expandableEventIndices = expandableEventIndices.filter((eventIndex) => {
      const endTimeMs = toJsTimestamp(this.events[eventIndex].endTime);
      return endTimeMs > now;
    });

    if (expandableEventIndices.length === 0) {
      return -1;
    }

    const in5Minutes = now + (5 * 60 * 1000);
    expandableEventIndices.sort(
        (a, b) => this.compareEventPriority_(a, b, in5Minutes));

    return expandableEventIndices[0];
  }

  protected hasDoubleBooked_() {
    return this.doubleBookedIndices_.length > 0;
  }

  protected isDoubleBooked_(index: number) {
    return this.doubleBookedIndices_.includes(index);
  }

  protected isExpanded_(index: number) {
    return index === this.expandedEventIndex_;
  }

  protected recordSeeMoreClick_() {
    this.dispatchEvent(new Event('usage', {composed: true, bubbles: true}));
    recordCalendarAction(CalendarAction.SEE_MORE_CLICKED, this.moduleName);
  }

  // Sort events to move expanded events before any of its double booked
  // events.
  protected sortEvents_() {
    const expandedEvent = this.events[this.expandedEventIndex_];
    const firstDoubleBookedEventIndex =
        this.events.findIndex((calendarEvent: CalendarEvent) => {
          return calendarEvent.startTime.internalValue ===
              expandedEvent.startTime.internalValue;
        });

    if (firstDoubleBookedEventIndex < this.expandedEventIndex_) {
      this.events.splice(this.expandedEventIndex_, 1);
      this.expandedEventIndex_ = firstDoubleBookedEventIndex;
      this.events.splice(this.expandedEventIndex_, 0, expandedEvent);
    }
  }
}

customElements.define(CalendarElement.is, CalendarElement);
