// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../module_header.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {I18nMixinLit, loadTimeData} from '../../../i18n_setup.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import {getHtml} from './outlook_calendar_module.html.js';

export interface OutlookCalendarModuleElement {
  $: {
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

  protected getMenuItemGroups_(): MenuItem[][] {
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

  protected onDisableButtonClick_() {
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

  protected onMenuButtonClick_(e: Event) {
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
