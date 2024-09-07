// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_chip/cr_chip.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CalendarEvent} from '../../../calendar_data.mojom-webui.js';
import {I18nMixinLit} from '../../../i18n_setup.js';
import {WindowProxy} from '../../../window_proxy.js';

import {getCss} from './calendar_event.css.js';
import {getHtml} from './calendar_event.html.js';
import {CalendarAction, recordCalendarAction, toJsTimestamp} from './common.js';

const kAttachmentScrollFadeBuffer: number = 4;
const kMillisecondsInMinute: number = 60000;
const kMinutesInHour: number = 60;

export interface CalendarEventElement {
  $: {
    header: HTMLAnchorElement,
    startTime: HTMLElement,
    timeStatus: HTMLElement,
    title: HTMLElement,
  };
}

const CalendarEventElementBase = I18nMixinLit(CrLitElement);

/**
 * The calendar event element for displaying a single event.
 */
export class CalendarEventElement extends CalendarEventElementBase {
  static get is() {
    return 'ntp-calendar-event';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      doubleBooked: {type: Boolean},
      event: {type: Object},

      expanded: {
        type: Boolean,
        reflect: true,
      },

      index: {type: Number},
      moduleName: {type: String},
      attachmentListClass_: {type: String},
      formattedStartTime_: {type: String},
      timeStatus_: {type: String},
    };
  }

  doubleBooked: boolean;
  event: CalendarEvent;
  expanded: boolean;
  index: number;
  moduleName: string;

  protected attachmentListClass_: string;
  protected formattedStartTime_: string;
  protected intersectionObserver_: IntersectionObserver;
  protected timeStatus_: string;

  override updated(changedProperties: PropertyValues<this>) {
    if ((changedProperties.has('event') || changedProperties.has('expanded')) &&
        (this.expanded && this.showAttachments_())) {
      const attachmentList = this.renderRoot.querySelector('#attachmentList');
      if (attachmentList && attachmentList.children.length > 1) {
        this.intersectionObserver_ =
            new IntersectionObserver(() => this.updateAttachmentListClass_(), {
              root: attachmentList,
              threshold: 1.0,
            });
        this.intersectionObserver_.observe(attachmentList.children[0]);
        this.intersectionObserver_.observe(
            attachmentList.children[attachmentList.children.length - 1]);
      }
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('event')) {
      this.formattedStartTime_ = this.computeFormattedStartTime_();
    }

    if (changedProperties.has('event') || changedProperties.has('expanded')) {
      this.timeStatus_ = this.computeTimeStatus_();
    }
  }

  private computeFormattedStartTime_(): string {
    const offsetDate = toJsTimestamp(this.event.startTime);
    const dateObj = new Date(offsetDate);
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
    const startTime = toJsTimestamp(this.event.startTime);
    // Current time in milliseconds since Windows epoch.
    const now = WindowProxy.getInstance().now().valueOf();

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

  protected openAttachment_(e: Event) {
    this.dispatchEvent(new Event('usage', {composed: true, bubbles: true}));
    recordCalendarAction(CalendarAction.ATTACHMENT_CLICKED, this.moduleName);
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    WindowProxy.getInstance().navigate(
        this.event.attachments[index]!.resourceUrl.url);
  }

  protected openVideoConference_() {
    this.dispatchEvent(new Event('usage', {composed: true, bubbles: true}));
    recordCalendarAction(
        CalendarAction.CONFERENCE_CALL_CLICKED, this.moduleName);
    WindowProxy.getInstance().navigate(this.event.conferenceUrl!.url);
  }

  protected recordHeaderClick_() {
    this.dispatchEvent(new Event('usage', {composed: true, bubbles: true}));
    let action = CalendarAction.BASIC_EVENT_HEADER_CLICKED;
    if (this.expanded) {
      action = CalendarAction.EXPANDED_EVENT_HEADER_CLICKED;
    } else if (this.doubleBooked) {
      action = CalendarAction.DOUBLE_BOOKED_EVENT_HEADER_CLICKED;
    }
    recordCalendarAction(action, this.moduleName);
    chrome.metricsPrivate.recordSmallCount(
        `NewTabPage.${this.moduleName}.EventClickIndex`, this.index);
  }

  protected showConferenceButton_(): boolean {
    return !!this.event.conferenceUrl?.url;
  }

  protected showAttachments_(): boolean {
    return this.event.attachments.length > 0;
  }

  protected showLocation_(): boolean {
    return !!this.event.location;
  }

  protected updateAttachmentListClass_() {
    const attachmentList = this.renderRoot.querySelector('#attachmentList');
    if (!attachmentList) {
      this.attachmentListClass_ = '';
      return;
    }
    const scrollableRight =
        (attachmentList!.scrollWidth - attachmentList!.scrollLeft -
         kAttachmentScrollFadeBuffer) > attachmentList!.clientWidth;
    const scrollableLeft =
        attachmentList!.scrollLeft - kAttachmentScrollFadeBuffer > 0;

    if (scrollableRight && scrollableLeft) {
      this.attachmentListClass_ = 'scrollable';
    } else if (scrollableRight) {
      this.attachmentListClass_ = 'scrollable-right';
    } else if (scrollableLeft) {
      this.attachmentListClass_ = 'scrollable-left';
    } else {
      this.attachmentListClass_ = '';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-calendar-event': CalendarEventElement;
  }
}

customElements.define(CalendarEventElement.is, CalendarEventElement);
