// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format on
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format off

export interface LifetimeBrowserProxy {
  // Triggers a browser restart.
  restart(): void;

  // Triggers a browser relaunch.
  relaunch(): void;

  // <if expr="chromeos">
  // First signs out current user and then performs a restart.
  signOutAndRestart(): void;

  /**
   * Triggers a factory reset. The parameter indicates whether to install a
   * TPM firmware update (if available) after the reset.
   */
  factoryReset(requestTpmFirmwareUpdate: boolean): void;
  // </if>
}

export class LifetimeBrowserProxyImpl implements LifetimeBrowserProxy {
  restart() {
    chrome.send('restart');
  }

  relaunch() {
    chrome.send('relaunch');
  }

  // <if expr="chromeos">
  signOutAndRestart() {
    chrome.send('signOutAndRestart');
  }

  factoryReset(requestTpmFirmwareUpdate: boolean) {
    chrome.send('factoryReset', [requestTpmFirmwareUpdate]);
  }
  // </if>
}

addSingletonGetter(LifetimeBrowserProxyImpl);
