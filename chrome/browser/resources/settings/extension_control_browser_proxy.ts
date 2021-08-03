// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

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
}

addSingletonGetter(ExtensionControlBrowserProxyImpl);
