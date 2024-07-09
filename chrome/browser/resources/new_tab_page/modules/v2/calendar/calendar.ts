// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './calendar_event.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CalendarEvent} from '../../../google_calendar.mojom-webui.js';
import {WindowProxy} from '../../../window_proxy.js';

import {getTemplate} from './calendar.html.js';
import {toJsTimestamp} from './common.js';

export interface CalendarElement {
  $: {
    seeMore: HTMLDivElement,
  };
}

/**
 * The calendar element for displaying the user's list of events. .
 */
export class CalendarElement extends PolymerElement {
  static get is() {
    return 'ntp-calendar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      calendarLink: String,
      events: Object,
      expandedEventIndex_: {
        type: Number,
        computed: 'computeExpandedEventIndex_(events)',
      },
    };
  }

  calendarLink: string;
  events: CalendarEvent[];

  private expandedEventIndex_: number;

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

  private isExpanded_(index: number) {
    return index === this.expandedEventIndex_;
  }
}

customElements.define(CalendarElement.is, CalendarElement);
