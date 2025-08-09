// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome.browser is a private namespace reserved for chrome://webui-browser.
// It is injected into the page by WebUIBrowserRendererExtension.
declare namespace chrome {
  export namespace browser {
    // Attaches a guest contents to an iframe on the page.
    export function attachIframeGuest(
        guestContentsId: number, contentWindow: Window): void;
  }
}
