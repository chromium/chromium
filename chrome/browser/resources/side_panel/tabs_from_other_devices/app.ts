// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ForeignSession, ForeignSessionPageHandlerRemote, ForeignSessionTab} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import {ForeignSessionPageCallbackRouter, ForeignSessionPageHandler} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export type TabInfo = ForeignSessionTab&{
  sessionTag: string,
  screenshotUrl?: string,
};

const TabsFromOtherDevicesAppElementBase = CrLitElement;

export class TabsFromOtherDevicesAppElement extends
    TabsFromOtherDevicesAppElementBase {
  static get is() {
    return 'tabs-from-other-devices-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      syncedDevices_: {type: Array},
      searchQuery_: {type: String},
      selectedDeviceTag_: {type: String},
      failedScreenshots_: {type: Object},
      showScreenshots_: {type: Boolean},
    };
  }

  protected accessor syncedDevices_: ForeignSession[] = [];
  protected accessor searchQuery_: string = '';
  protected accessor selectedDeviceTag_: string|null = null;
  protected accessor failedScreenshots_: Set<string> = new Set();
  protected accessor showScreenshots_: boolean =
      loadTimeData.getBoolean('showScreenshots');

  private foreignSessionHandler: ForeignSessionPageHandlerRemote =
      ForeignSessionPageHandler.getRemote();
  private foreignSessionCallbackRouter: ForeignSessionPageCallbackRouter =
      new ForeignSessionPageCallbackRouter();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().refreshColorsCss();

    this.foreignSessionCallbackRouter.onForeignSessionsChanged.addListener(
        (sessionList: ForeignSession[]) =>
            this.setForeignSessions_(sessionList));

    this.foreignSessionHandler.getForeignSessions().then(
        (res: {sessions: ForeignSession[]}) => {
          this.setForeignSessions_(res.sessions);
          this.foreignSessionHandler.setPage(
              this.foreignSessionCallbackRouter.$.bindNewPipeAndPassRemote());
        });
  }

  private setForeignSessions_(sessions: ForeignSession[]) {
    this.syncedDevices_ = sessions;
    if (!this.selectedDeviceTag_ && sessions.length > 0) {
      this.selectedDeviceTag_ = sessions[0]!.tag;
    }
    // Also re-try fetching any screenshots that previously failed - it's
    // possible they are now available.
    this.failedScreenshots_ = new Set();
  }

  protected onDeviceSelectClick_(e: MouseEvent) {
    const menu =
        this.shadowRoot.querySelector<CrActionMenuElement>('#deviceMenu')!;
    menu.showAt(e.target as HTMLElement);
  }

  protected onDeviceItemClick_(e: MouseEvent) {
    const target = e.currentTarget as HTMLElement;
    this.selectedDeviceTag_ = target.dataset['tag']!;
    const menu =
        this.shadowRoot.querySelector<CrActionMenuElement>('#deviceMenu')!;
    menu.close();
  }

  protected getSelectedDeviceName_(): string {
    const device =
        this.syncedDevices_.find(d => d.tag === this.selectedDeviceTag_);
    return device ? device.name : '';
  }

  protected onTabAuxclick_(e: MouseEvent) {
    this.onTabClick_(e);
  }

  protected onTabClick_(e: MouseEvent) {
    if (e.button > 1) {
      return;
    }

    const target = e.currentTarget as HTMLElement;
    const sessionTag = target.dataset['sessionTag']!;
    const tabId = parseInt(target.dataset['tabId']!, 10);

    const modifiers: ClickModifiers = {
      middleButton: e.button === 1,
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    };
    this.foreignSessionHandler.openForeignSessionTab(
        sessionTag, tabId, modifiers);
  }

  protected getHostname_(urlStr: string): string {
    try {
      return new URL(urlStr).hostname;
    } catch (e) {
      return urlStr;
    }
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.searchQuery_ = e.detail;
  }

  private createTabInfo_(tab: ForeignSessionTab, sessionTag: string): TabInfo {
    return {
      ...tab,
      sessionTag,
      screenshotUrl: this.showScreenshots_ ?
          `chrome://synced-screenshot/${sessionTag}/${tab.sessionId}` +
              `?${tab.timestamp}` :
          undefined,
    };
  }

  // Returns the tabs that match the current user selection: If there is a
  // search query, this returns all tabs from all devices that match the search
  // query. Otherwise, returns all tabs for the selected device tag.
  protected getFilteredTabs_(): TabInfo[] {
    const tabs: TabInfo[] = [];
    if (this.searchQuery_) {
      // The user has entered a search query. Return all tabs matching the
      // query, from all devices. (The UI will hide the device picker dropdown
      // in this case.)
      const query = this.searchQuery_.trim().toLowerCase();
      for (const device of this.syncedDevices_) {
        for (const window of device.windows) {
          for (const tab of window.tabs) {
            if (tab.title.toLowerCase().includes(query) ||
                tab.url.toLowerCase().includes(query)) {
              tabs.push(this.createTabInfo_(tab, device.tag));
            }
          }
        }
      }
    } else {
      // No search query - return all tabs from the selected device.
      const device =
          this.syncedDevices_.find(d => d.tag === this.selectedDeviceTag_);
      if (device) {
        for (const window of device.windows) {
          for (const tab of window.tabs) {
            tabs.push(this.createTabInfo_(tab, device.tag));
          }
        }
      }
    }
    return tabs;
  }

  protected screenshotLoadFailed_(sessionTag: string, tabId: number): boolean {
    return this.failedScreenshots_.has(`${sessionTag}_${tabId}`);
  }

  protected onScreenshotError_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const sessionTag = target.dataset['sessionTag']!;
    const tabId = target.dataset['tabId']!;
    const key = `${sessionTag}_${tabId}`;
    // Note: Need to recreate the set to make Lit react to the change.
    const newFailed = new Set(this.failedScreenshots_);
    newFailed.add(key);
    this.failedScreenshots_ = newFailed;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabs-from-other-devices-app': TabsFromOtherDevicesAppElement;
  }
}

customElements.define(
    TabsFromOtherDevicesAppElement.is, TabsFromOtherDevicesAppElement);
