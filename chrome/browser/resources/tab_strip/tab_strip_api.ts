// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {TabsObserverCallbackRouter, TabStripService} from './tab_strip_api.mojom-webui.js';
import type {Position, TabStripServiceRemote} from './tab_strip_api.mojom-webui.js';

export interface TabStripApiProxy {
  createTabAt(pos: Position|null, url: Url|null): Promise<boolean>;
}

export class TabStripApiProxyImpl implements TabStripApiProxy {
  service: TabStripServiceRemote = TabStripService.getRemote();
  observer: TabsObserverCallbackRouter = new TabsObserverCallbackRouter();

  createTabAt(pos: Position|null, url: Url|null) {
    return this.service.createTabAt(pos, url);
  }

  static getInstance(): TabStripApiProxy {
    return instance || (instance = new TabStripApiProxyImpl());
  }
}

let instance: TabStripApiProxy|null = null;
