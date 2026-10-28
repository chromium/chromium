// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabStripService} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {TabStripServiceInterface} from '/tab_strip_api/tab_strip_api.mojom-webui.js';

import {PageCallbackRouter, PageHandlerFactory} from './tabstrip_ntb.mojom-webui.js';

export class BrowserProxy {
  tabStripService: TabStripServiceInterface;
  callbackRouter: PageCallbackRouter;

  constructor(
      tabStripService: TabStripServiceInterface,
      callbackRouter: PageCallbackRouter = new PageCallbackRouter()) {
    this.tabStripService = tabStripService;
    this.callbackRouter = callbackRouter;
  }

  static getInstance(): BrowserProxy {
    if (!instance) {
      const callbackRouter = new PageCallbackRouter();
      PageHandlerFactory.getRemote().bindPage(
          callbackRouter.$.bindNewPipeAndPassRemote());
      instance = new BrowserProxy(TabStripService.getRemote(), callbackRouter);
    }
    return instance;
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
