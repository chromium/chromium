// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WebViewType} from './web_view_type.js';

interface WebViewWithPartition extends chrome.webviewTag.WebView {
  partition: string;
}

export class WindowManager {
  private mockWebviews: Set<WebViewType> = new Set();

  constructor(private mainWebview: WebViewType) {
    this.mainWebview.addEventListener('newwindow', (e: Event) => {
      this.onNewWindow(e as unknown as chrome.webviewTag.NewWindowEvent);
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

    e.window.attach(newWebview as unknown as {[key: string]: void});

    newWebview.addEventListener('close', () => {
      this.mockWebviews.delete(newWebview);
      if (newWebview.parentNode) {
        newWebview.remove();
      }
    });

    this.mockWebviews.add(newWebview);

    // Adding to the DOM is required for the navigation to not abort. Add with
    // `display: none` to prevent actually rendering
    newWebview.style.display = 'none';
    document.body.appendChild(newWebview);
  }
}
