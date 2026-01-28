// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/js/cr.js';

import {
  BrowserControlsFactory,
  BrowserControlsObserverCallbackRouter,
  BrowserControlsServiceRemote,
} from './browser_controls_api.mojom-webui.js';
import type {BrowserControlsServiceInterface} from './browser_controls_api.mojom-webui.js';
import {ClickDispositionFlag, ContextMenuState, ContextMenuType, DevToolsState, NavigationState} from './browser_controls_api_data_model.mojom-webui.js';

export {
  ClickDispositionFlag,
  ContextMenuState,
  ContextMenuType,
  DevToolsState,
  NavigationState,
};

export interface BrowserProxy {
  callbackRouter: BrowserControlsObserverCallbackRouter;
  handler: BrowserControlsServiceInterface;

  /**
   * Records a value in a histogram.
   * @param histogramName The name of the histogram.
   * @param value The value to record.
   * @param maxValue The maximum value of the histogram.
   */
  recordInHistogram(histogramName: string, value: number, maxValue: number):
      void;
}

export class BrowserProxyImpl implements BrowserProxy {
  callbackRouter: BrowserControlsObserverCallbackRouter;
  handler: BrowserControlsServiceInterface;

  private constructor() {
    this.callbackRouter = new BrowserControlsObserverCallbackRouter();
    this.handler = new BrowserControlsServiceRemote();
    BrowserControlsFactory.getRemote().createBrowserControls(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as BrowserControlsServiceRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  /**
   * Records a value in a histogram.
   * @param histogramName The name of the histogram.
   * @param value The value to record.
   * @param maxValue The maximum value of the histogram.
   */
  recordInHistogram(histogramName: string, value: number, maxValue: number) {
    chrome.send(
        'metricsHandler:recordInHistogram', [histogramName, value, maxValue]);
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(proxy: BrowserProxy) {
    instance = proxy;
  }
}

let instance: BrowserProxy|null = null;
