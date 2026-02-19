// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/js/cr.js';

import {BrowserControlsObserverCallbackRouter, BrowserControlsService} from './browser_controls_api.mojom-webui.js';
import type {BrowserControlsServiceInterface} from './browser_controls_api.mojom-webui.js';
import {ClickDispositionFlag, ContextMenuType} from './browser_controls_api_data_model.mojom-webui.js';
import type {NavigationControlsState, ReloadControlState} from './browser_controls_api_data_model.mojom-webui.js';

export {
  ClickDispositionFlag,
  ContextMenuType,
};
export type {
  NavigationControlsState,
  ReloadControlState,
};

export type NavigationControlsStateListener =
    (state: NavigationControlsState) => void;

export type NavigationControlsStateListenerHandle = number;
export const INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE:
    NavigationControlsStateListenerHandle = -1;

export interface BrowserProxy {
  handler: BrowserControlsServiceInterface;

  /**
   * Records a value in a histogram.
   * @param histogramName The name of the histogram.
   * @param value The value to record.
   * @param maxValue The maximum value of the histogram.
   */
  recordInHistogram(histogramName: string, value: number, maxValue: number):
      void;

  addNavigationStateListener(listener: NavigationControlsStateListener):
      NavigationControlsStateListenerHandle;

  removeNavigationStateListener(handle: NavigationControlsStateListenerHandle):
      void;
}

export class BrowserProxyImpl implements BrowserProxy {
  private callbackRouter: BrowserControlsObserverCallbackRouter;
  handler: BrowserControlsServiceInterface;

  private constructor() {
    this.callbackRouter = new BrowserControlsObserverCallbackRouter();
    this.handler = BrowserControlsService.getRemote();
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

  addNavigationStateListener(listener: NavigationControlsStateListener) {
    const handle =
        this.callbackRouter.onNavigationControlsStateChanged.addListener(
            listener);
    this.handler.bind().then(fence => {
      listener(fence.state);
      this.callbackRouter.$.bindHandle(fence.updateStream.handle);
    });
    return handle;
  }

  removeNavigationStateListener(handle: NavigationControlsStateListenerHandle) {
    if (handle !== INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE) {
      this.callbackRouter.removeListener(handle);
    }
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(proxy: BrowserProxy) {
    instance = proxy;
  }
}

let instance: BrowserProxy|null = null;
