// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ExtensionControlBrowserProxy {
  // TODO(dbeam): should be be returning !Promise<boolean> to indicate whether
  // it succeeded?
  disableExtension(extensionId: string): void;

  manageExtension(extensionId: string): void;
}

export class ExtensionControlBrowserProxyImpl implements
    ExtensionControlBrowserProxy {
  disableExtension(extensionId: string) {
    chrome.send('disableExtension', [extensionId]);
  }

  manageExtension(extensionId: string) {
    window.open('chrome://extensions?id=' + extensionId);
  }

  static getInstance(): ExtensionControlBrowserProxy {
    return instance || (instance = new ExtensionControlBrowserProxyImpl());
  }

  static setInstance(obj: ExtensionControlBrowserProxy) {
    instance = obj;
  }
}

let instance: ExtensionControlBrowserProxy|null = null;
