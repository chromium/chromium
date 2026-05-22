// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import type {WebViewType} from './web_view_type.js';

interface WebViewWithPartition extends chrome.webviewTag.WebView {
  partition: string;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  [key: string]: any;
}

function generateUnguessableToken(): string {
  const array = new Uint8Array(16);
  crypto.getRandomValues(array);
  return Array.from(array, byte => byte.toString(16).padStart(2, '0'))
      .join('')
      .toUpperCase();
}

export class WindowManager {
  /**
   * Stores dynamically created, hidden `<webview>` elements used to host and
   * track guest windows opened via `window.open()`. Keyed by their unique
   * unguessable token ID. We must keep references to these elements to manage
   * their lifecycle (e.g., removing them from the DOM when closed).
   */
  private trackedWebviews: Map<string, WebViewType> = new Map();

  constructor(private mainWebview: WebViewType) {
    this.mainWebview.addEventListener('newwindow', (e: Event) => {
      this.onNewWindow(e as chrome.webviewTag.NewWindowEvent);
    });

    // Register listener for calls from C++
    const browserProxy = BrowserProxyImpl.getInstance();
    browserProxy.callbackRouter.onWindowClosed.addListener(
        (windowId: {value: string}) => {
          this.onWindowClosedFromBrowser(windowId.value);
        });
  }

  private onNewWindow(e: chrome.webviewTag.NewWindowEvent) {
    e.preventDefault();

    // Create a new <webview> that will be attached to the window.open call in
    // the AIM page. This <webview> can then be used as a communication channel
    // back to the original window.open call.
    const newWebview = document.createElement('webview') as HTMLElement as
        WebViewWithPartition;
    newWebview.partition = 'persist:contextual-tasks';

    e.window.attach(newWebview);

    // Generate a random 128-bit unguessable token formatted as a 32-character
    // uppercase hex string to satisfy Mojo validation constraints.
    const windowId = generateUnguessableToken();

    // Register window with C++
    const url = new URL(window.location.href);
    const taskIdStr = url.searchParams.get('chrome_task_id');
    if (taskIdStr) {
      const browserProxy = BrowserProxyImpl.getInstance();
      browserProxy.handler.registerWindow(
          {value: {value: taskIdStr}},
          e.targetUrl,
          {value: windowId},
      );
    }

    newWebview.addEventListener('close', () => {
      this.trackedWebviews.delete(windowId);
      if (newWebview.parentNode) {
        newWebview.remove();
      }
      // Request C++ to close the actual window
      const browserProxy = BrowserProxyImpl.getInstance();
      browserProxy.handler.closeWindow({value: windowId});
    });

    this.trackedWebviews.set(windowId, newWebview as unknown as WebViewType);

    // Adding to the DOM is required for the navigation to not abort. Add with
    // `display: none` to prevent actually rendering
    newWebview.style.display = 'none';
    document.body.appendChild(newWebview);
  }

  private onWindowClosedFromBrowser(windowId: string) {
    const webview = this.trackedWebviews.get(windowId);
    if (webview) {
      this.trackedWebviews.delete(windowId);
      if ((webview as unknown as HTMLElement).parentNode) {
        (webview as unknown as HTMLElement).remove();
      }
    }
  }
}
