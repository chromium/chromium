// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '../tab_search_item.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {normalizeURL, TabData, TabItemType} from '../tab_data.js';
import type {ProfileData, Tab, TabsRemovedInfo, TabUpdateInfo} from '../tab_search.mojom-webui.js';
import type {TabSearchApiProxy} from '../tab_search_api_proxy.js';
import {TabSearchApiProxyImpl} from '../tab_search_api_proxy.js';
import type {TabSearchItemElement} from '../tab_search_item.js';
import {tabHasMediaAlerts} from '../tab_search_utils.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class SplitNewTabPageAppElement extends CrLitElement {
  static get is() {
    return 'split-new-tab-page-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      allInvisibleTabs_: {type: Array},
    };
  }

  protected accessor allInvisibleTabs_: TabData[] = [];
  private activeTabId_: number = -1;
  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();

    if (loadTimeData.getBoolean('splitViewEnabled')) {
      this.apiProxy_.getIsSplit().then(({isSplit}) => {
        if (!isSplit) {
          this.redirectToNtp_();
        }
      });
    } else {
      this.redirectToNtp_();
    }

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.tabsChanged.addListener(this.onTabsChanged_.bind(this)),
        callbackRouter.tabUpdated.addListener(this.onTabUpdated_.bind(this)),
        callbackRouter.tabsRemoved.addListener(this.onTabsRemoved_.bind(this)),
        callbackRouter.tabUnsplit.addListener(this.redirectToNtp_.bind(this)));

    this.apiProxy_.getProfileData().then(({profileData}) => {
      this.onTabsChanged_(profileData);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
    this.listenerIds_ = [];
  }

  protected onClose_() {
    // Close should never be triggered from an inactive tab, so this should
    // always close the tab hosting this WebUI.
    assert(this.activeTabId_ >= 0);
    this.apiProxy_.closeTab(this.activeTabId_);
  }

  protected onTabClick_(e: Event) {
    const target = e.currentTarget as TabSearchItemElement;
    this.apiProxy_.replaceActiveSplitTab((target.data.tab as Tab).tabId);
  }

  private onTabsChanged_(profileData: ProfileData) {
    const activeWindow = profileData.windows.find(({active}) => active)!;
    this.activeTabId_ = activeWindow.tabs.find((tab) => tab.active)!.tabId;
    this.allInvisibleTabs_ =
        activeWindow.tabs.filter(tab => !tab.visible)
            .map(tab => this.getTabData_(tab, true, TabItemType.OPEN_TAB));
    this.sortTabs_();
  }

  private onTabUpdated_(tabUpdateInfo: TabUpdateInfo) {
    const {tab, inActiveWindow} = tabUpdateInfo;
    if (!inActiveWindow) {
      return;
    }

    const tabData = this.getTabData_(tab, inActiveWindow, TabItemType.OPEN_TAB);
    const tabIndex =
        this.allInvisibleTabs_.findIndex(el => el.tab.tabId === tab.tabId);
    if (tabIndex >= 0) {
      this.allInvisibleTabs_[tabIndex] = tabData;
    } else {
      this.allInvisibleTabs_.push(tabData);
    }
    this.sortTabs_();
  }

  private onTabsRemoved_(tabsRemovedInfo: TabsRemovedInfo) {
    if (this.allInvisibleTabs_.length === 0) {
      return;
    }

    const ids = new Set(tabsRemovedInfo.tabIds);
    this.allInvisibleTabs_ =
        this.allInvisibleTabs_.filter(tab => !ids.has(tab.tab.tabId));
    this.sortTabs_();
  }

  private sortTabs_() {
    this.allInvisibleTabs_.sort((a, b) => {
      const tabA = a.tab as Tab;
      const tabB = b.tab as Tab;
      // Move tabs with media alerts to the top of the list.
      if (tabHasMediaAlerts(tabA) && !tabHasMediaAlerts(tabB)) {
        return -1;
      } else if (!tabHasMediaAlerts(tabA) && tabHasMediaAlerts(tabB)) {
        return 1;
      }
      return (tabB.lastActiveTimeTicks && tabA.lastActiveTimeTicks) ?
          Number(
              tabB.lastActiveTimeTicks.internalValue -
              tabA.lastActiveTimeTicks.internalValue) :
          0;
    });
  }

  private getTabData_(tab: Tab, inActiveWindow: boolean, type: TabItemType):
      TabData {
    const tabData =
        new TabData(tab, type, new URL(normalizeURL(tab.url.url)).hostname);

    if (type === TabItemType.OPEN_TAB) {
      tabData.inActiveWindow = inActiveWindow;
    }

    tabData.a11yTypeText = loadTimeData.getString('a11yOpenTab');

    return tabData;
  }

  private redirectToNtp_() {
    window.location.replace(loadTimeData.getString('newTabPageUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'split-new-tab-page-app': SplitNewTabPageAppElement;
  }
}

customElements.define(SplitNewTabPageAppElement.is, SplitNewTabPageAppElement);
