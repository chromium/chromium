// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_chip/cr_chip.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';

import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Attachment, CalendarEvent} from '../../../google_calendar.mojom-webui.js';
import {I18nMixin} from '../../../i18n_setup.js';

import {getTemplate} from './calendar_event.html.js';

// Microseconds between windows and unix epoch.
const kWindowsToUnixEpochOffset: bigint = 11644473600000000n;
const kMillisecondsInMinute: number = 60000;
const kMinutesInHour: number = 60;

export interface CalendarEventElement {
  $: {
    header: HTMLAnchorElement,
    startTime: HTMLSpanElement,
    timeStatus: HTMLSpanElement,
    title: HTMLSpanElement,
  };
}

/**
 * The calendar event element for displaying a single event.
 */
export class CalendarEventElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-calendar-event';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      event: Object,
      expanded: {
        type: Boolean,
        reflectToAttribute: true,
      },
      formattedStartTime_: {
        type: String,
        computed: 'computeFormattedStartTime_(event.startTime)',
      },
      timeStatus_: {
        type: String,
        computed: 'computeTimeStatus_(event.startTime, expanded)',
      },
    };
  }

  event: CalendarEvent;
  expanded: boolean;

  private formattedStartTime_: string;
  private timeStatus_: string;

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

  private computeTimeStatus_(): string {
    if (!this.expanded) {
      return '';
    }

    // Start time of event in milliseconds since Windows epoch.
    const startTime = Number(
        (this.event.startTime.internalValue - kWindowsToUnixEpochOffset) /
        1000n);
    // Current time in milliseconds since Windows epoch.
    const now = Date.now().valueOf();

    const minutesUntilMeeting =
        Math.round((startTime - now) / kMillisecondsInMinute);
    if (minutesUntilMeeting <= 0) {
      return this.i18n('modulesCalendarInProgress');
    }

    if (minutesUntilMeeting < kMinutesInHour) {
      return this.i18n('modulesCalendarInXMin', minutesUntilMeeting.toString());
    }

    const hoursUntilMeeting = minutesUntilMeeting / kMinutesInHour;
    return this.i18n(
        'modulesCalendarInXHr', Math.round(hoursUntilMeeting).toString());
  }

  private openAttachment_(e: DomRepeatEvent<Attachment>) {
    window.location.href = e.model.item.resourceUrl.url;
  }

  private openVideoConference_() {
    window.location.href = this.event.conferenceUrl!.url;
  }

  private showConferenceButton_(): boolean {
    return !!(this.event.conferenceUrl && this.event.conferenceUrl.url);
  }

  private showAttachments_(): boolean {
    return this.event.attachments.length > 0;
  }

  private showLocation_(): boolean {
    return !!this.event.location;
  }
}

customElements.define(CalendarEventElement.is, CalendarEventElement);
