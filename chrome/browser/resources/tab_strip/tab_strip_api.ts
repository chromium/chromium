// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {TabsObserverCallbackRouter, TabStripService} from './tab_strip_api.mojom-webui.js';
import type {Position, Tab, TabId, TabsSnapshot, TabStripServiceRemote} from './tab_strip_api.mojom-webui.js';

export interface TabStripApiProxy {
  createTabAt(pos: Position|null, url: Url|null): Promise<Tab>;
  getTabs(): Promise<TabsSnapshot>;
  getTab(id: TabId): Promise<Tab>;
  getCallbackRouter(): TabsObserverCallbackRouter;
}

export class TabStripApiProxyImpl implements TabStripApiProxy {
  service: TabStripServiceRemote = TabStripService.getRemote();
  observer: TabsObserverCallbackRouter = new TabsObserverCallbackRouter();

  createTabAt(pos: Position|null, url: Url|null) {
    return this.service.createTabAt(pos, url);
  }

  getTabs(): Promise<TabsSnapshot> {
    return this.service.getTabs();
  }

  getTab(id: TabId): Promise<Tab> {
    return this.service.getTab(id);
  }

  getCallbackRouter(): TabsObserverCallbackRouter {
    return this.observer;
  }

  static getInstance(): TabStripApiProxy {
    return instance || (instance = new TabStripApiProxyImpl());
  }
}

let instance: TabStripApiProxy|null = null;
