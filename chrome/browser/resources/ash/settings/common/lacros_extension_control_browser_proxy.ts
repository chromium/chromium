// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface LacrosExtensionControlBrowserProxy {
  manageLacrosExtension(extensionId: string): void;
}

let instance: LacrosExtensionControlBrowserProxy|null = null;

export class LacrosExtensionControlBrowserProxyImpl implements
    LacrosExtensionControlBrowserProxy {
  manageLacrosExtension(extensionId: string): void {
    chrome.send('openExtensionPageInLacros', [extensionId]);
  }

  static getInstance(): LacrosExtensionControlBrowserProxy {
    return instance ||
        (instance = new LacrosExtensionControlBrowserProxyImpl());
  }

  static setInstance(obj: LacrosExtensionControlBrowserProxy): void {
    instance = obj;
  }
}
