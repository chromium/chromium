// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CalendarEvent} from '../../../google_calendar.mojom-webui.js';

import {getTemplate} from './calendar_event.html.js';

// Microseconds between windows and unix epoch.
const kWindowsToUnixEpochOffset: bigint = 11644473600000000n;

export interface CalendarEventElement {
  $: {
    startTime: HTMLParagraphElement,
    title: HTMLParagraphElement,
  };
}

/**
 * The calendar event element for displaying a single event.
 */
export class CalendarEventElement extends PolymerElement {
  static get is() {
    return 'ntp-calendar-event';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      event: Object,
      formattedStartTime_: {
        type: String,
        computed: 'computeFormattedStartTime_(event.startTime)',
      },
    };
  }

  private formattedStartTime_: string;

  event: CalendarEvent;

  private computeFormattedStartTime_(): string {
    const offsetDate =
        (this.event.startTime.internalValue - kWindowsToUnixEpochOffset) /
        1000n;
    const dateObj = new Date(Number(offsetDate));
    let timeStr =
        Intl.DateTimeFormat(undefined, {timeStyle: 'short'}).format(dateObj);
    // Remove extra spacing and make AM/PM lower case.
    timeStr = timeStr.replace(' AM', 'am').replace(' PM', 'pm');
    return timeStr;
  }
}

customElements.define(CalendarEventElement.is, CalendarEventElement);
