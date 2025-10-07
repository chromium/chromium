// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './contextual_tasks.mojom-webui.js';

let instance: BrowserProxy|null = null;

export interface BrowserProxy {
  getCallbackRouter(): PageCallbackRouter;
  getThreadUrl(): Promise<{url: Url}>;
  showUi(): void;
}

export class BrowserProxyImpl implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerRemote;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  getThreadUrl() {
    return this.handler.getThreadUrl();
  }

  showUi() {
    this.handler.showUi();
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(proxy: BrowserProxy) {
    instance = proxy;
  }
}
