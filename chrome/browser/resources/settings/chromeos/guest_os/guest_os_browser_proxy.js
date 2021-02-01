// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @typedef {{guid: string,
 *            label: string,
 *            sharedWith: ?string,
 *            promptBeforeSharing: boolean}}
 */
/* #export */ let GuestOsSharedUsbDevice;

/* #export */ const CROSTINI_TYPE = 'crostini';
/* #export */ const PLUGIN_VM_TYPE = 'pluginVm';

/**
 * @fileoverview A helper object used by the both the Crostini and Plugin VM
 * sections to manage the file sharing and USB sharing.
 */
cr.define('settings', function() {
  /** @interface */
  /* #export */ class GuestOsBrowserProxy {
    /**
     * @param {!Array<string>} paths Paths to sanitze.
     * @return {!Promise<!Array<string>>} Text to display in UI.
     */
    getGuestOsSharedPathsDisplayText(paths) {}

    /**
     * @param {string} vmName VM to stop sharing path with.
     * @param {string} path Path to stop sharing.
     * @return {!Promise<boolean>} Result of unsharing.
     */
    removeGuestOsSharedPath(vmName, path) {}

    /** Called when page is ready. */
    notifyGuestOsSharedUsbDevicesPageReady() {}

    /**
     * @param {string} vmName VM to share device with.
     * @param {string} guid Unique device identifier.
     * @param {boolean} shared Whether device is currently shared with Crostini.
     */
    setGuestOsUsbDeviceShared(vmName, guid, shared) {}
  }

  /** @implements {settings.GuestOsBrowserProxy} */
  /* #export */ class GuestOsBrowserProxyImpl {
    /** @override */
    getGuestOsSharedPathsDisplayText(paths) {
      return cr.sendWithPromise('getGuestOsSharedPathsDisplayText', paths);
    }

    /** @override */
    removeGuestOsSharedPath(vmName, path) {
      return cr.sendWithPromise('removeGuestOsSharedPath', vmName, path);
    }

    /** @override */
    notifyGuestOsSharedUsbDevicesPageReady() {
      return chrome.send('notifyGuestOsSharedUsbDevicesPageReady');
    }

    /** @override */
    setGuestOsUsbDeviceShared(vmName, guid, shared) {
      return chrome.send('setGuestOsUsbDeviceShared', [vmName, guid, shared]);
    }
  }

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(GuestOsBrowserProxyImpl);

  // #cr_define_end
  return {
    GuestOsBrowserProxy: GuestOsBrowserProxy,
    GuestOsBrowserProxyImpl: GuestOsBrowserProxyImpl,
  };
});
