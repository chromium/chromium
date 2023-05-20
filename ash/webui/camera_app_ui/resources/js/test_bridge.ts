// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @file
 * This scripts should only be loaded as a SharedWorker and the worker is only
 * used for communication between Tast tests and CCA instance. Generally, the
 * SharedWorker will first be created by Tast tests when constructing the test
 * bridge. And when CCA is launched, it will also connect to the SharedWorker
 * during its initialization. And the SharedWorker will be close once the test
 * bridge is destroyed from Tast tests side.
 */

import {AppWindow} from './app_window.js';
import {assert} from './assert.js';
import * as Comlink from './lib/comlink.js';

/**
 * Pending unbound AppWindow requested by tast waiting to be bound by next
 * launched CCA window.
 */
let pendingAppWindow: AppWindow|null = null;

/**
 * Whether the app is launched from a cold start. It will be set to false once
 * an app instance is launched.
 */
let fromColdStart = true;

/**
 * Registers a pending unbound AppWindow which will be bound with the URL
 * later once the window is created. This method is expected to be called in
 * Tast tests.
 */
export function registerUnboundWindow(): AppWindow&Comlink.ProxyMarked {
  assert(pendingAppWindow === null);
  const appWindow = new AppWindow(fromColdStart);
  pendingAppWindow = appWindow;
  return Comlink.proxy(appWindow);
}

/**
 * Binds the URL to pending AppWindow and exposes AppWindow using the URL.
 *
 * @param url The URL to bind.
 */
function bindWindow(url: string): (AppWindow&Comlink.ProxyMarked)|null {
  fromColdStart = false;
  if (pendingAppWindow !== null) {
    const appWindow = pendingAppWindow;
    pendingAppWindow = null;
    appWindow.bindUrl(url);
    return Comlink.proxy(appWindow);
  }
  return null;
}

// This is needed since we currently have the same tsconfig for files running
// in SharedWorker and in CCA.
// TODO(b/213408699): Remove this after the tsconfig are separated.
// eslint-disable-next-line @typescript-eslint/consistent-type-assertions
const sharedWorkerScope = self as SharedWorkerGlobalScope;

export interface TestBridge {
  bindWindow: typeof bindWindow;
  registerUnboundWindow: typeof registerUnboundWindow;
}

/**
 * Triggers when the Shared Worker is connected.
 */
sharedWorkerScope.onconnect = (event: MessageEvent) => {
  const port = event.ports[0];
  Comlink.expose(
      {
        bindWindow,
        registerUnboundWindow,
      },
      port);
  port.start();
};
