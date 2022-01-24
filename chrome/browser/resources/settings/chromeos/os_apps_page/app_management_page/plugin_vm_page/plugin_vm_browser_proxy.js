// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview A helper object used by the Plugin VM section
 * to manage the Plugin VM.
 */
/** @interface */
export class PluginVmBrowserProxy {
  /**
   * @return {!Promise<boolean>} Whether Plugin VM needs to be relaunched for
   *     permissions to take effect.
   */
  isRelaunchNeededForNewPermissions() {}

  /**
   * Relaunches Plugin VM.
   */
  relaunchPluginVm() {}
}

/** @implements {PluginVmBrowserProxy} */
export class PluginVmBrowserProxyImpl {
  /** @override */
  isRelaunchNeededForNewPermissions() {
    return sendWithPromise('isRelaunchNeededForNewPermissions');
  }

  /** @override */
  relaunchPluginVm() {
    chrome.send('relaunchPluginVm');
  }
}

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
addSingletonGetter(PluginVmBrowserProxyImpl);
