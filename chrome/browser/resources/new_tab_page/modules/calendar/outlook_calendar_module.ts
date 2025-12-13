// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './calendar.js';
import '../info_dialog.js';
import '../module_header.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CalendarEvent} from '../../calendar_data.mojom-webui.js';
import {I18nMixinLit, loadTimeData} from '../../i18n_setup.js';
import type {OutlookCalendarPageHandlerRemote} from '../../outlook_calendar.mojom-webui.js';
import {ParentTrustedDocumentProxy} from '../microsoft_auth_frame_connector.js';
import {ModuleDescriptor} from '../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import type {CalendarElement} from './calendar.js';
import {getHtml} from './outlook_calendar_module.html.js';
import {OutlookCalendarProxyImpl} from './outlook_calendar_proxy.js';

export interface OutlookCalendarModuleElement {
  $: {
    calendar: CalendarElement,
    moduleHeaderElementV2: ModuleHeaderElement,
  };
}

const OutlookCalendarModuleElementBase = I18nMixinLit(CrLitElement);

/**
 * The Outlook Calendar module, which serves as an inside look in to upcoming
 * events on a user's Microsoft Outlook calendar.
 */
export class OutlookCalendarModuleElement extends
    OutlookCalendarModuleElementBase {
  static get is() {
    return 'ntp-outlook-calendar-module';
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

  protected accessor events_: CalendarEvent[] = [];
  protected accessor showInfoDialog_: boolean = false;

  private handler_: OutlookCalendarPageHandlerRemote;

  constructor(events: CalendarEvent[]) {
    super();
    this.handler_ = OutlookCalendarProxyImpl.getInstance().handler;
    this.events_ = events;
  }

  protected getMenuItems_(): MenuItem[] {
    return [
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
        text: this.i18n('modulesOutlookCalendarDisableButtonText'),
      },
      {
        action: 'signout',
        icon: 'modules:logout',
        text: this.i18n('modulesMicrosoftSignOutButtonText'),
      },
      {
        action: 'info',
        icon: 'modules:info',
        text: this.i18n('moduleInfoButtonTitle'),
      },
    ];
  }

  protected onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesOutlookCalendarTitle')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  protected onInfoButtonClick_() {
    this.showInfoDialog_ = true;
  }

  protected onInfoDialogClose_() {
    this.showInfoDialog_ = false;
  }

  protected onDismissButtonClick_() {
    this.handler_.dismissModule();
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: this.i18n('modulesOutlookCalendarDismissToastMessage'),
        restoreCallback: () => this.handler_.restoreModule(),
      },
    }));
  }

  protected onSignOutButtonClick_() {
    ParentTrustedDocumentProxy.getInstance()?.getChildDocument().signOut();
  }
}


customElements.define(
    OutlookCalendarModuleElement.is, OutlookCalendarModuleElement);

async function createOutlookCalendarElement():
    Promise<OutlookCalendarModuleElement|null> {
  const {events} =
      await OutlookCalendarProxyImpl.getInstance().handler.getEvents();
  return events.length > 0 ? new OutlookCalendarModuleElement(events) : null;
}

export const outlookCalendarDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id*/ 'outlook_calendar', createOutlookCalendarElement);
