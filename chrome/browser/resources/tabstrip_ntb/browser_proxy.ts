// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabStripService} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {TabStripServiceInterface} from '/tab_strip_api/tab_strip_api.mojom-webui.js';

export class BrowserProxy {
  tabStripService: TabStripServiceInterface;

  constructor(tabStripService: TabStripServiceInterface) {
    this.tabStripService = tabStripService;
  }

  static getInstance(): BrowserProxy {
    return instance ||
        (instance = new BrowserProxy(TabStripService.getRemote()));
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
