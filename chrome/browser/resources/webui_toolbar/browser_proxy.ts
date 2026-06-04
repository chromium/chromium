// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/js/cr.js';

import {BrowserControlsService} from './browser_controls_api.mojom-webui.js';
import type {BrowserControlsServiceInterface} from './browser_controls_api.mojom-webui.js';
import {EventDispositionFlag} from './browser_controls_api_data_model.mojom-webui.js';
import {ToolbarUIObserverCallbackRouter, ToolbarUIService} from './toolbar_ui_api.mojom-webui.js';
import type {ToolbarUIServiceInterface} from './toolbar_ui_api.mojom-webui.js';
import {ContextMenuType} from './toolbar_ui_api_data_model.mojom-webui.js';
import type {BackForwardButtonState, FocusRequestTarget, IconUpdate, NavigationControlsState, OmniboxViewState, ReloadControlState} from './toolbar_ui_api_data_model.mojom-webui.js';

export {
  ContextMenuType,
  EventDispositionFlag,
};
export type {
  BackForwardButtonState,
  IconUpdate,
  NavigationControlsState,
  OmniboxViewState,
  ReloadControlState,
};

export type NavigationControlsStateListener =
    (icons: IconUpdate[], state: NavigationControlsState) => void;

export type NavigationControlsStateListenerHandle = number;
export const INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE:
    NavigationControlsStateListenerHandle = -1;
/*
 * Listener type for invocations to
 *    toolbar_ui_api.mojom.ToolbarUIObserver.OnFocusRequested method.
 *
 * Declare handle:
 *   listenerHandle : FocusRequestHandle = INVALID_FOCUS_REQUEST_HANDLE;
 *
 * Subscribe:
 *   listenerHandle = browserProxy.addFocusRequestListener(
 *       (target: FocusRequestTarget) => {
 *   });
 *
 * Unsubscribe:
 *   browserProxy.removeFocusRequestListener(listenerHandle);
 *
 * Calling removeFocus with INVALID_FOCUS_REQUEST_HANDLE is OK; so it's
 * the preferred initialization value for FocusRequestHandle type.
 */
export type FocusRequestListener = (target: FocusRequestTarget) => void;
export type FocusRequestHandle = number;
export const INVALID_FOCUS_REQUEST_HANDLE: FocusRequestHandle = -1;

export interface BrowserProxy {
  browserControlsHandler: BrowserControlsServiceInterface;
  toolbarUIHandler: ToolbarUIServiceInterface;

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

  addFocusRequestListener(listener: FocusRequestListener): FocusRequestHandle;

  removeNavigationStateListener(handle: NavigationControlsStateListenerHandle):
      void;
  removeFocusRequestListener(handle: FocusRequestHandle): void;
}

export class BrowserProxyImpl implements BrowserProxy {
  private callbackRouter: ToolbarUIObserverCallbackRouter;
  browserControlsHandler: BrowserControlsServiceInterface;
  toolbarUIHandler: ToolbarUIServiceInterface;

  private constructor() {
    this.callbackRouter = new ToolbarUIObserverCallbackRouter();
    this.browserControlsHandler = BrowserControlsService.getRemote();
    this.toolbarUIHandler = ToolbarUIService.getRemote();
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
    this.toolbarUIHandler.bind().then(fence => {
      listener(fence.icons, fence.state);
      this.callbackRouter.$.bindHandle(fence.updateStream.handle);
    });
    return handle;
  }

  addFocusRequestListener(listener: FocusRequestListener) {
    // This assumes addNavigationStateListener will happen or has happened to
    // actually connect the router.
    return this.callbackRouter.onFocusRequested.addListener(listener);
  }

  removeNavigationStateListener(handle: NavigationControlsStateListenerHandle) {
    if (handle !== INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE) {
      this.callbackRouter.removeListener(handle);
    }
  }

  removeFocusRequestListener(handle: FocusRequestHandle) {
    if (handle !== INVALID_FOCUS_REQUEST_HANDLE) {
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
