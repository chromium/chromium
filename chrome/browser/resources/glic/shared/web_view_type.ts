// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This library allows you to obtain a chrome.webviewTab.WebView element when
// extensions are enabled, or a SlimWebViewElement when they are not.

// <if expr="not enable_extensions_core">
import '/shared/guest_view/slim_web_view.js';

// </if>
// Importing a type doesn't have a runtime effect, because these imports are
// removed at compile time. We add this import just to satisfy the type checker.
import type {SlimWebViewElement} from '/shared/guest_view/slim_web_view.js';

export type WebViewType = chrome.webviewTag.WebView|SlimWebViewElement;

export function isFullWebView(webview: WebViewType):
    webview is chrome.webviewTag.WebView {
  // Bypass field check because WebView is added dynamically to the window
  // object.
  return webview.constructor === (window as any).WebView;
}
