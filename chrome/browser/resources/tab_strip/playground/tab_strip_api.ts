// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Empty} from '//resources/mojo/mojo/public/mojom/base/empty.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {TabsObserverCallbackRouter, TabStripService} from '../tab_strip_api.mojom-webui.js';
import type {Position, Tab, TabId, TabsSnapshot, TabStripServiceRemote} from '../tab_strip_api.mojom-webui.js';

export interface TabStripApiProxy {
  getTabs(): Promise<TabsSnapshot>;
  getTab(id: TabId): Promise<Tab>;
  createTabAt(pos: Position|null, url: Url|null): Promise<Tab>;
  closeTabs(tabs: TabId[]): Promise<Empty>;
  activateTab(tab: TabId): Promise<Empty>;
  getCallbackRouter(): TabsObserverCallbackRouter;
}

export class TabStripApiProxyImpl implements TabStripApiProxy {
  service: TabStripServiceRemote = TabStripService.getRemote();
  observer: TabsObserverCallbackRouter = new TabsObserverCallbackRouter();

  getTabs(): Promise<TabsSnapshot> {
    return this.service.getTabs();
  }

  getTab(id: TabId): Promise<Tab> {
    return this.service.getTab(id);
  }

  createTabAt(pos: Position|null, url: Url|null) {
    return this.service.createTabAt(pos, url);
  }

  closeTabs(tabs: TabId[]) {
    return this.service.closeTabs(tabs);
  }

  activateTab(tab: TabId) {
    return this.service.activateTab(tab);
  }

  getCallbackRouter(): TabsObserverCallbackRouter {
    return this.observer;
  }

  static getInstance(): TabStripApiProxy {
    return instance || (instance = new TabStripApiProxyImpl());
  }
}

let instance: TabStripApiProxy|null = null;
