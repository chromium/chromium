// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser proxy for document distillation, AXTree node hierarchy navigation,
// selection tracking, DOM anchor mapping, and content rendering callbacks.
export interface ContentBrowserProxy {
  onConnected(): void;
}

export class ContentBrowserProxyImpl implements ContentBrowserProxy {
  onConnected(): void {
    chrome.readingMode.onConnected();
  }

  static getInstance(): ContentBrowserProxy {
    return instance || (instance = new ContentBrowserProxyImpl());
  }

  static setInstance(obj: ContentBrowserProxy) {
    instance = obj;
  }
}

let instance: ContentBrowserProxy|null = null;
