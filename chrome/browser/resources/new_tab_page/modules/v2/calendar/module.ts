// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../module_header.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin} from '../../../i18n_setup.js';
import {ModuleDescriptor} from '../../module_descriptor.js';

import {getTemplate} from './module.html.js';

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
    return {};
  }
}

customElements.define(CalendarModuleElement.is, CalendarModuleElement);

async function createCalendarElement(): Promise<CalendarModuleElement|null> {
  return new Promise<CalendarModuleElement>(
      (resolve) => resolve(new CalendarModuleElement()));
}

export const googleCalendarDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id*/ 'google_calendar', createCalendarElement);
