// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import { addSingletonGetter,sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @fileoverview Helper browser proxy for peripheral data access client.
 */

/**
 * @typedef {{
 *     prefName: string,
 *     isUserConfigurable: boolean
 * }}
 */
export let DataAccessPolicyState;

/** @interface */
export class PeripheralDataAccessBrowserProxy {
  /**
   * @return {!Promise<boolean>}
   * Returns true if the device supports thunderbolt peripherals.
   */
  isThunderboltSupported() {}

  /**
   * @return {!Promise<DataAccessPolicyState>}
   * Returns the status of the policy,
   * kDeviceDevicePciPeripheralDataAccessEnabled.
   */
  getPolicyState() {}
}

/** @implements {PeripheralDataAccessBrowserProxy} */
export class PeripheralDataAccessBrowserProxyImpl {
  /**
   * @override
   * @return {!Promise<boolean>}
   */
  isThunderboltSupported() {
    return sendWithPromise('isThunderboltSupported');
  }

  /**
   * @override
   * @return {!Promise<DataAccessPolicyState>}
   */
  getPolicyState() {
    return sendWithPromise('getPolicyState');
  }
}

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
addSingletonGetter(PeripheralDataAccessBrowserProxyImpl);
