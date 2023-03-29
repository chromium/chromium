// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class that enables substituting custom extension control
 * logic in test cases. Also see the equivalent
 * extension_control_browser_proxy.ts in settings.
 */

export interface ExtensionControlBrowserProxy {
  disableExtension(extensionId: string): void;
}

export class ExtensionControlBrowserProxyImpl implements
    ExtensionControlBrowserProxy {
  disableExtension(extensionId: string) {
    chrome.send('disableExtension', [extensionId]);
  }

  static getInstance(): ExtensionControlBrowserProxy {
    return instance || (instance = new ExtensionControlBrowserProxyImpl());
  }

  static setInstance(obj: ExtensionControlBrowserProxy) {
    instance = obj;
  }
}

let instance: ExtensionControlBrowserProxy|null = null;
