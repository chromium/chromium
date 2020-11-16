// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @typedef {{
 *   deviceName: string,
 * }}
 */
/* #export */ let DeviceNameMetadata;

/** @interface */
/* #export */ class DeviceNameBrowserProxy {
  /**
   * Queries the system for metadata about the device name.
   * @return {!Promise<!DeviceNameMetadata>}
   */
  getDeviceNameMetadata() {}
}

/**
 * @implements {DeviceNameBrowserProxy}
 */
/* #export */ class DeviceNameBrowserProxyImpl {
  /** @override */
  getDeviceNameMetadata() {
    return cr.sendWithPromise('getDeviceNameMetadata');
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
cr.addSingletonGetter(DeviceNameBrowserProxyImpl);
