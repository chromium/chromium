// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {sendWithPromise, addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @fileoverview Helper browser proxy for peripheral data access client.
 */

cr.define('settings', function() {
  /** @interface */
  /* #export */ class PeripheralDataAccessBrowserProxy {
    /**
     * @return {!Promise<boolean>}
     * Returns true if the device supports thunderbolt peripherals.
     */
    isThunderboltSupported() {}
  }

  /** @implements {settings.PeripheralDataAccessBrowserProxy} */
  /* #export */ class PeripheralDataAccessBrowserProxyImpl {
    /**
     * @override
     * @return {!Promise<boolean>}
     */
    isThunderboltSupported() {
      return cr.sendWithPromise('isThunderboltSupported');
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(PeripheralDataAccessBrowserProxyImpl);

  // #cr_define_end
  return {
    PeripheralDataAccessBrowserProxy: PeripheralDataAccessBrowserProxy,
    PeripheralDataAccessBrowserProxyImpl: PeripheralDataAccessBrowserProxyImpl,
  };
});
