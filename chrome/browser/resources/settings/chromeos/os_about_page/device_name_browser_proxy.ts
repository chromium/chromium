// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import {DeviceNameState, SetDeviceNameResult} from './device_name_util.js';

export interface DeviceNameMetadata {
  deviceName: string;
  deviceNameState: DeviceNameState;
}

export interface DeviceNameBrowserProxy {
  /**
   * Notifies the system that the page is ready for the device name.
   */
  notifyReadyForDeviceName(): Promise<DeviceNameMetadata>;

  /**
   * Attempts to set the device name to the new name entered by the user.
   */
  attemptSetDeviceName(name: string): Promise<SetDeviceNameResult>;
}

let instance: DeviceNameBrowserProxy|null = null;

export class DeviceNameBrowserProxyImpl implements DeviceNameBrowserProxy {
  static getInstance(): DeviceNameBrowserProxy {
    return instance || (instance = new DeviceNameBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: DeviceNameBrowserProxy): void {
    instance = obj;
  }

  notifyReadyForDeviceName(): Promise<DeviceNameMetadata> {
    return sendWithPromise('notifyReadyForDeviceName');
  }

  attemptSetDeviceName(name: string): Promise<SetDeviceNameResult> {
    return sendWithPromise('attemptSetDeviceName', name);
  }
}
