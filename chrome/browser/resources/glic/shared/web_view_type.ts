// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This library allows you to obtain a chrome.webviewTag.WebView element when
// extensions are enabled, or a SlimWebviewElement when they are not.

// <if expr="not enable_extensions_core">
import '/shared/guest_view/slim_webview.js';
import type {SlimWebviewElement} from '/shared/guest_view/slim_webview.js';
// </if>

// <if expr="not enable_extensions_core">
// Need to include both types here to address type errors within
// 'if (isFullWebView(..)) {...}' blocks since the two types don't have
// identical APIs yet.
export type WebViewType = chrome.webviewTag.WebView|SlimWebviewElement;
// </if>

// <if expr="enable_extensions_core">
export type WebViewType = chrome.webviewTag.WebView;

declare global {
  interface HTMLElementTagNameMap {
    'webview': chrome.webviewTag.WebView;
  }
}
// </if>

export function isFullWebView(webview: WebViewType):
    webview is chrome.webviewTag.WebView {
  // Bypass field check because WebView is added dynamically to the window
  // object.
  return webview.constructor === (window as any).WebView;
}
