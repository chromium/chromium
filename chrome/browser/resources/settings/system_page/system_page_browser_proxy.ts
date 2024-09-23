// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communication for the system page. */

// clang-format on
import {loadTimeData} from '../i18n_setup.js';
// clang-format off

export interface SystemPageBrowserProxy {
  /** Shows the native system proxy settings. */
  showProxySettings(): void;

  /**
   * @return Whether hardware acceleration was enabled when the user
   *     started Chrome.
   */
  wasHardwareAccelerationEnabledAtStartup(): boolean;
}

export class SystemPageBrowserProxyImpl implements SystemPageBrowserProxy {
  showProxySettings() {
    chrome.send('showProxySettings');
  }

  wasHardwareAccelerationEnabledAtStartup() {
    return loadTimeData.getBoolean('hardwareAccelerationEnabledAtStartup');
  }

  static getInstance(): SystemPageBrowserProxy {
    return instance || (instance = new SystemPageBrowserProxyImpl());
  }

  static setInstance(obj: SystemPageBrowserProxy) {
    instance = obj;
  }
}

let instance: SystemPageBrowserProxy|null = null;
