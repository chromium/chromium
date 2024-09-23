// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './calendar.js';
import '../../module_header.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CalendarEvent} from '../../../calendar_data.mojom-webui.js';
import type {GoogleCalendarPageHandlerRemote} from '../../../google_calendar.mojom-webui.js';
import {I18nMixinLit} from '../../../i18n_setup.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import type {CalendarElement} from './calendar.js';
import {getCss} from './google_calendar_module.css.js';
import {getHtml} from './google_calendar_module.html.js';
import {GoogleCalendarProxyImpl} from './google_calendar_proxy.js';

export interface GoogleCalendarModuleElement {
  $: {
    calendar: CalendarElement,
    moduleHeaderElementV2: ModuleHeaderElement,
  };
}

const GoogleCalendarModuleElementBase = I18nMixinLit(CrLitElement);

/**
 * The Google Calendar module, which serves as an inside look in to today's
 * events on a user's Google Calendar .
 */
export class GoogleCalendarModuleElement extends
    GoogleCalendarModuleElementBase {
  static get is() {
    return 'ntp-google-calendar-module';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      events_: {type: Object},
      showInfoDialog_: {type: Boolean},
    };
  }

  protected events_: CalendarEvent[];
  protected showInfoDialog_: boolean;

  private handler_: GoogleCalendarPageHandlerRemote;

  constructor(events: CalendarEvent[]) {
    super();
    this.handler_ = GoogleCalendarProxyImpl.getInstance().handler;
    this.events_ = events;
  }

  protected getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'dismiss',
          icon: 'modules:visibility_off',
          text: this.i18nRecursive(
              '', 'modulesDismissForHoursButtonText',
              'calendarModuleDismissHours'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18n('modulesGoogleCalendarDisableButtonText'),
        },
        {
          action: 'info',
          icon: 'modules:info',
          text: this.i18n('moduleInfoButtonTitle'),
        },
      ],
      [
        {
          action: 'customize-module',
          icon: 'modules:tune',
          text: this.i18n('modulesCustomizeButtonText'),
        },
      ],
    ];
  }

  protected onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: this.i18n('modulesGoogleCalendarDisableToastMessage'),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  protected onDismissButtonClick_() {
    this.handler_.dismissModule();
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: this.i18n('modulesGoogleCalendarDismissToastMessage'),
        restoreCallback: this.handler_.restoreModule,
      },
    }));
  }

  protected onInfoButtonClick_() {
    this.showInfoDialog_ = true;
  }

  protected onInfoDialogClose_() {
    this.showInfoDialog_ = false;
  }

  protected onMenuButtonClick_(e: Event) {
    this.$.moduleHeaderElementV2.showAt(e);
  }
}

customElements.define(
    GoogleCalendarModuleElement.is, GoogleCalendarModuleElement);

async function createGoogleCalendarElement():
    Promise<GoogleCalendarModuleElement|null> {
  const {events} =
      await GoogleCalendarProxyImpl.getInstance().handler.getEvents();
  return events.length > 0 ? new GoogleCalendarModuleElement(events) : null;
}

export const googleCalendarDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id*/ 'google_calendar', createGoogleCalendarElement);
