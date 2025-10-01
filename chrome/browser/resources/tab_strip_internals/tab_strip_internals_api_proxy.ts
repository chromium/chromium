// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './tab_strip_internals.mojom-webui.js';
import type {Container} from './tab_strip_internals.mojom-webui.js';

/**
 * Declares methods exposed by TabStripInternals API Proxy.
 */
export interface TabStripInternalsApiProxy {
  /**
   * Fetches the current TabStrip state from the browser.
   */
  getTabStripData(): Promise<{data: Container}>;

  /**
   * Returns the callback router for listening to browser-sent updates.
   */
  getCallbackRouter(): PageCallbackRouter;
}

/**
 * Implementation of TabStripInternalsApiProxy using Mojo bindings.
 */
export class TabStripInternalsApiProxyImpl implements
    TabStripInternalsApiProxy {
  // Used to listen to browser-sent events.
  callbackRouter: PageCallbackRouter;
  // User to invoke functions browser-side functionality.
  handler: PageHandlerRemote;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): TabStripInternalsApiProxy {
    return instance || (instance = new TabStripInternalsApiProxyImpl());
  }

  static setInstance(obj: TabStripInternalsApiProxy) {
    instance = obj;
  }

  /**
   * Fetch data from the browser (C++ backend).
   */
  getTabStripData() {
    return this.handler.getTabStripData();
  }

  /**
   * Listen to updates from the browser (C++ backend).
   */
  getCallbackRouter() {
    return this.callbackRouter;
  }
}

let instance: TabStripInternalsApiProxy|null = null;
