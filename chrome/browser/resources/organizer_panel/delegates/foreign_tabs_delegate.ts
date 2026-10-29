// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '../icons.html.js';
import '../organizer_list_section_item.js';

import {CrUrlListItemSize} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {assert} from '//resources/js/assert.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {BrowserProxy, ForeignTab} from '../foreign_tabs.mojom-webui.js';
import {browserProxyFactory} from '../foreign_tabs.mojom-webui.js';
import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';

import {getHostnameOrUrl} from './tab_delegate_utils.js';

export class ForeignTabsDelegate implements
    OrganizerListSectionDelegate<ForeignTab> {
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private shouldHide_: boolean = !loadTimeData.getBoolean('isUserSignedIn') ||
      !loadTimeData.getBoolean('diceSupportEnabled');

  init(_sectionClient: OrganizerListSectionClient) {}

  getId(): string {
    return 'cross-device-tabs';
  }

  getHeader(): string {
    return loadTimeData.getString('tabsOnOtherDevices');
  }

  shouldHide(): boolean {
    return this.shouldHide_;
  }

  async getItems(): Promise<Array<OrganizerListSectionItem<ForeignTab>>> {
    if (!loadTimeData.getBoolean('isUserSignedIn')) {
      this.shouldHide_ = true;
      return [];
    }

    const {tabs} = await this.browserProxy_.handler.getForeignTabs();

    if (!loadTimeData.getBoolean('diceSupportEnabled') && tabs.length === 0) {
      this.shouldHide_ = true;
    } else {
      this.shouldHide_ = false;
    }

    return tabs.map(
        tab => ({
          title: [tab.title],
          description: [
            {text: tab.lastActiveElapsedText, matchable: false},
            {text: tab.deviceName},
          ],
          extraSearchTerms: tab.url ? [getHostnameOrUrl(tab.url)] : undefined,
          prefixIcon: tab.url ? {url: tab.url} : undefined,
          data: tab,
        }));
  }

  getZeroState(): TemplateResult {
    // TODO(crbug.com/576361077): Add support for signed out users.
    // TODO(crbug.com/579400988): Call ShouldStartShowingCrossDeviceSigninPromo
    // before showing this zero state.
    const item: OrganizerListSectionItem<unknown> = {
      title: [loadTimeData.getString('signInOnPhone')],
      prefixIcon: {
        element: html`<cr-icon icon="organizer-panel:login"
            style="--iron-icon-height: 20px; --iron-icon-width: 20px;">
        </cr-icon>`,
      },
      size: CrUrlListItemSize.COMPACT,
    };
    return html`
      <organizer-list-section-item .item="${item}"
          @click="${() => this.onShowCrossDeviceSignInPromoClick_()}">
      </organizer-list-section-item>
    `;
  }

  private onShowCrossDeviceSignInPromoClick_() {
    this.browserProxy_.handler.showCrossDeviceSignInPromo();
  }

  onItemClick(item: OrganizerListSectionItem<ForeignTab>) {
    const data = item.data;
    assert(data);
    this.browserProxy_.handler.openTab(data.sessionTag, data.sessionTabId);
  }
}
