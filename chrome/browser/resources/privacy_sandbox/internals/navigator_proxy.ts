// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class NavigatorProxy {
  writeToClipboard(text: string): Promise<void> {
    return navigator.clipboard.writeText(text);
  }

  static getInstance(): NavigatorProxy {
    return instance || (instance = new NavigatorProxy());
  }

  static setInstance(proxy: NavigatorProxy) {
    instance = proxy;
  }
}

let instance: NavigatorProxy|null = null;
