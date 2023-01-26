// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper browser proxy for peripheral data access client.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface DataAccessPolicyState {
  prefName: string;
  isUserConfigurable: boolean;
}

export interface PeripheralDataAccessBrowserProxy {
  /**
   * Returns true if the device supports thunderbolt peripherals.
   */
  isThunderboltSupported(): Promise<boolean>;

  /**
   * Returns the status of the policy,
   * kDeviceDevicePciPeripheralDataAccessEnabled.
   */
  getPolicyState(): Promise<DataAccessPolicyState>;
}

let instance: PeripheralDataAccessBrowserProxy|null = null;

export class PeripheralDataAccessBrowserProxyImpl implements
    PeripheralDataAccessBrowserProxy {
  static getInstance(): PeripheralDataAccessBrowserProxy {
    return instance || (instance = new PeripheralDataAccessBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: PeripheralDataAccessBrowserProxy): void {
    instance = obj;
  }

  isThunderboltSupported(): Promise<boolean> {
    return sendWithPromise('isThunderboltSupported');
  }

  getPolicyState(): Promise<DataAccessPolicyState> {
    return sendWithPromise('getPolicyState');
  }
}
