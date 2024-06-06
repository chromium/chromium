// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './calendar.js';
import '../../module_header.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CalendarEvent, GoogleCalendarPageHandlerRemote} from '../../../google_calendar.mojom-webui.js';
import {I18nMixin} from '../../../i18n_setup.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElementV2} from '../module_header.js';

import type {CalendarElement} from './calendar.js';
import {getTemplate} from './google_calendar_module.html.js';
import {GoogleCalendarProxyImpl} from './google_calendar_proxy.js';

export interface GoogleCalendarModuleElement {
  $: {
    calendar: CalendarElement,
    moduleHeaderElementV2: ModuleHeaderElementV2,
  };
}

/**
 * The Google Calendar module, which serves as an inside look in to today's
 * events on a user's Google Calendar .
 */
export class GoogleCalendarModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-google-calendar-module';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      events_: Object,
    };
  }

private events_:
  CalendarEvent[];

private handler_:
  GoogleCalendarPageHandlerRemote;

  constructor(events: CalendarEvent[]) {
    super();
    this.handler_ = GoogleCalendarProxyImpl.getInstance().handler;
    this.events_ = events;
  }

  private getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'dismiss',
          icon: 'modules:visibility_off',
          text: this.i18n('modulesGoogleCalendarDismissButtonText'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18n('modulesGoogleCalendarDisableButtonText'),
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

  private onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: this.i18n('modulesGoogleCalendarDisableToastMessage'),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  private onDismissButtonClick_() {
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

  private onMenuButtonClick_(e: Event) {
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
