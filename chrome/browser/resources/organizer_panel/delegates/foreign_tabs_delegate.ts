// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy, ForeignTab} from '../foreign_tabs.mojom-webui.js';
import {browserProxyFactory} from '../foreign_tabs.mojom-webui.js';
import type {OrganizerAction, OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';
import {SectionId} from '../organizer_panel.mojom-webui.js';

import {getCompactZeroState, getHostnameOrUrl, getTabPrefixIcon} from './tab_delegate_utils.js';

export class ForeignTabsDelegate implements
    OrganizerListSectionDelegate<ForeignTab> {
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private shouldHide_: boolean = true;
  private onShowCrossDeviceSignInPromoClick_ = () => {
    this.browserProxy_.handler.showCrossDeviceSignInPromo();
  };

  init(_sectionClient: OrganizerListSectionClient) {}

  getId(): SectionId {
    return SectionId.kCrossDeviceTabs;
  }

  getHeader(): string {
    return loadTimeData.getString('tabsOnOtherDevices');
  }

  shouldHide(): boolean {
    return this.shouldHide_;
  }

  async getItems(): Promise<Array<OrganizerListSectionItem<ForeignTab>>> {
    const {tabs, canShowCrossDeviceSignInPromo} =
        await this.browserProxy_.handler.getForeignTabs();

    this.shouldHide_ = tabs.length === 0 && !canShowCrossDeviceSignInPromo;

    return tabs.map(
        tab => ({
          title: [tab.title],
          description: [
            {text: tab.lastActiveElapsedText, matchable: false},
            {text: tab.deviceName},
          ],
          extraSearchTerms: tab.url ? [getHostnameOrUrl(tab.url)] : undefined,
          prefixIcon: getTabPrefixIcon(tab.url),
          data: tab,
        }));
  }

  getZeroState(): TemplateResult {
    return getCompactZeroState(
        loadTimeData.getString('signInOnPhone'), 'organizer-panel:login',
        this.onShowCrossDeviceSignInPromoClick_);
  }

  onItemClick(item: OrganizerListSectionItem<ForeignTab>) {
    chrome.metricsPrivate.recordUserAction('OrganizerPanel.OpenCrossDeviceTab');
    const data = item.data;
    assert(data);
    this.browserProxy_.handler.openTab(data.sessionTag, data.sessionTabId);
  }

  getItemClickAction(): OrganizerAction {
    return 'OpenCrossDeviceTab';
  }
}
