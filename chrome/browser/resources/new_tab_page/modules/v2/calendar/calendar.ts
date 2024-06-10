// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './calendar_event.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CalendarEvent} from '../../../google_calendar.mojom-webui.js';

import {getTemplate} from './calendar.html.js';

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
        value: 0,
      },
    };
  }

  calendarLink: string;
  events: CalendarEvent[];

  private expandedEventIndex_: number;

  private isExpanded_(index: number) {
    return index === this.expandedEventIndex_;
  }
}

customElements.define(CalendarElement.is, CalendarElement);
