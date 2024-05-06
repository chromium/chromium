// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../module_header.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElementV2} from '../module_header.js';

import {getTemplate} from './outlook_calendar_module.html.js';

export interface OutlookCalendarModuleElement {
  $: {
    moduleHeaderElementV2: ModuleHeaderElementV2,
  };
}

/**
 * The Outlook Calendar module, which serves as an inside look in to upcoming
 * events on a user's Microsoft Outlook calendar.
 */
export class OutlookCalendarModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-outlook-calendar-module';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18n('modulesOutlookCalendarDisableButtonText'),
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
            loadTimeData.getString('modulesOutlookCalendarTitle')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  private onMenuButtonClick_(e: Event) {
    this.$.moduleHeaderElementV2.showAt(e);
  }
}

customElements.define(
    OutlookCalendarModuleElement.is, OutlookCalendarModuleElement);

async function createOutlookCalendarElement():
    Promise<OutlookCalendarModuleElement|null> {
  return new Promise<OutlookCalendarModuleElement>(
      (resolve) => resolve(new OutlookCalendarModuleElement()));
}

export const outlookCalendarDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id*/ 'outlook_calendar', createOutlookCalendarElement);
