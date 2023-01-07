// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * strings.m.js is generated when we enable it via UseStringsJs() in webUI
 * controller. When loading it, it will populate data such as localized strings
 * into |window.loadTimeData|.
 */
import '/strings.m.js';

import {AppWindow} from './app_window.js';
import * as Comlink from './lib/comlink.js';
import {TestBridge} from './test_bridge.js';


declare global {
  interface Window {
    // TODO(crbug.com/980846): Refactor to use a better way rather than window
    // properties to pass data to other modules.
    appWindow: Comlink.Remote<AppWindow>|null;
    windowCreationTime: number;
  }
}


document.addEventListener('DOMContentLoaded', async () => {
  const workerPath = '/js/test_bridge.js';
  const sharedWorker = new SharedWorker(workerPath, {type: 'module'});
  const testBridge = Comlink.wrap<TestBridge>(sharedWorker.port);
  const appWindow = await testBridge.bindWindow(window.location.href);
  window.appWindow = appWindow;
  window.windowCreationTime = performance.now();
  if (appWindow !== null) {
    await appWindow.waitUntilReadyOnTastSide();
  }

  // Dynamically import the error module here so that the codes can be counted
  // by coverage report.
  const errorModule = await import('./error.js');
  errorModule.initialize();

  const mainScript = document.createElement('script');
  mainScript.setAttribute('type', 'module');
  mainScript.setAttribute('src', '/js/main.js');
  document.head.appendChild(mainScript);
});
