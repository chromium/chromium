// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

// LINT.IfChange(FormFactor)
export enum SyncedDeviceFormFactor {
  UNKNOWN = 0,
  DESKTOP = 1,
  PHONE = 2,
  TABLET = 3,
  AUTOMOTIVE = 4,
  WEARABLE = 5,
  TV = 6,
}
// LINT.ThenChange(//components/sync_device_info/device_info.h:FormFactor)

export interface DeviceTabVisibilityEntry {
  sessionTag: string;
  deviceName: string;
  lastActiveTime: string;
  formFactor: SyncedDeviceFormFactor;
  visible: boolean;
}

export interface DeviceTabVisibilityBrowserProxy {
  getDeviceTabVisibilityList(): Promise<DeviceTabVisibilityEntry[]>;
  setDeviceTabVisibility(sessionTag: string, visible: boolean): void;
}

export class DeviceTabVisibilityBrowserProxyImpl implements
    DeviceTabVisibilityBrowserProxy {
  getDeviceTabVisibilityList() {
    return sendWithPromise<DeviceTabVisibilityEntry[]>(
        'getDeviceTabVisibilityList');
  }

  setDeviceTabVisibility(sessionTag: string, visible: boolean) {
    chrome.send('setDeviceTabVisibility', [sessionTag, visible]);
  }

  static getInstance(): DeviceTabVisibilityBrowserProxy {
    return instance || (instance = new DeviceTabVisibilityBrowserProxyImpl());
  }

  static setInstance(obj: DeviceTabVisibilityBrowserProxy) {
    instance = obj;
  }
}

let instance: DeviceTabVisibilityBrowserProxy|null = null;
