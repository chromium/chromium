// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the Plugin VM section
 * to manage the Plugin VM.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface PluginVmBrowserProxy {
  isRelaunchNeededForNewPermissions(): Promise<boolean>;
  relaunchPluginVm(): void;
}

let instance: PluginVmBrowserProxy|null = null;

export class PluginVmBrowserProxyImpl implements PluginVmBrowserProxy {
  static getInstance(): PluginVmBrowserProxy {
    return instance || (instance = new PluginVmBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: PluginVmBrowserProxy): void {
    instance = obj;
  }

  isRelaunchNeededForNewPermissions(): Promise<boolean> {
    return sendWithPromise('isRelaunchNeededForNewPermissions');
  }

  relaunchPluginVm(): void {
    chrome.send('relaunchPluginVm');
  }
}
