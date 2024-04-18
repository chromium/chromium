// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../module_header.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElementV2} from '../module_header.js';

import {getTemplate} from './module.html.js';

export enum CalendarSource {
  GOOGLE,
  OUTLOOK,
}

export interface CalendarModuleElement {
  $: {
    moduleHeaderElementV2: ModuleHeaderElementV2,
  };
}

/**
 * The Calendar module, which serves as an inside look in to upcoming events on
 * a user's Google Calendar or Microsoft Outlook.
 */
export class CalendarModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-calendar-module';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      calendarSource_: {
        type: Object,
        observer: 'setSourceText_',
      },
      sourceDisableText_: String,
      sourceTitle_: String,
    };
  }

  private calendarSource_: CalendarSource;
  private sourceDisableText_: string = '';
  private sourceTitle_: string = '';

  constructor(calendarSource: CalendarSource) {
    super();
    this.calendarSource_ = calendarSource;
    this.setSourceText_();
  }

  private setSourceText_() {
    switch(this.calendarSource_) {
      case CalendarSource.GOOGLE:
        this.sourceTitle_ = this.i18n('modulesGoogleCalendarTitle');
        this.sourceDisableText_ =
            this.i18n('modulesGoogleCalendarDisableButtonText');
        break;
      case CalendarSource.OUTLOOK:
        this.sourceTitle_ = this.i18n('modulesOutlookCalendarTitle');
        this.sourceDisableText_ =
            this.i18n('modulesOutlookCalendarDisableButtonText');
        break;
    }
  }

  private getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.sourceDisableText_,
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
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            this.sourceTitle_),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  private onMenuButtonClick_(e: Event) {
    this.$.moduleHeaderElementV2.showAt(e);
  }
}

customElements.define(CalendarModuleElement.is, CalendarModuleElement);

async function createCalendarElement(calendarSource: CalendarSource):
    Promise<CalendarModuleElement|null> {
  return new Promise<CalendarModuleElement>(
      (resolve) => resolve(new CalendarModuleElement(calendarSource)));
}

export const googleCalendarDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id*/ 'google_calendar',
    () => createCalendarElement(CalendarSource.GOOGLE));

export const outlookCalendarDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id*/ 'outlook_calendar',
    () => createCalendarElement(CalendarSource.OUTLOOK));
