// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @typedef {{guid: string,
 *            label: string,
 *            guestId: ?GuestId,
 *            vendorId: string,
 *            productId: string,
 *            promptBeforeSharing: boolean}}
 */
export let GuestOsSharedUsbDevice;

export const CROSTINI_TYPE = 'crostini';
export const PLUGIN_VM_TYPE = 'pluginVm';

/**
 * Non-js key names are kept to match c++ style keys in prefs.
 * @typedef {{vm_name: string,
 *            container_name: string}}
 */
export let GuestId;

/**
 * |ipv4| below is null if the guest is not currently running.
 * @typedef {{id: !GuestId,
 *            ipv4: ?string}}
 */
export let ContainerInfo;

/**
 * @fileoverview A helper object used by the both the Crostini and Plugin VM
 * sections to manage the file sharing and USB sharing.
 */
  /** @interface */
export class GuestOsBrowserProxy {
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
   * @param {string} containerName container to share device with.
   * @param {string} guid Unique device identifier.
   * @param {boolean} shared Whether device is currently shared with Crostini.
   */
  setGuestOsUsbDeviceShared(vmName, containerName, guid, shared) {}
}

/** @type {?GuestOsBrowserProxy} */
let instance = null;

/**
 * @implements {GuestOsBrowserProxy}
 */
export class GuestOsBrowserProxyImpl {
  /** @return {!GuestOsBrowserProxy} */
  static getInstance() {
    return instance || (instance = new GuestOsBrowserProxyImpl());
  }

  /** @param {!GuestOsBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  getGuestOsSharedPathsDisplayText(paths) {
    return sendWithPromise('getGuestOsSharedPathsDisplayText', paths);
  }

  /** @override */
  removeGuestOsSharedPath(vmName, path) {
    return sendWithPromise('removeGuestOsSharedPath', vmName, path);
  }

  /** @override */
  notifyGuestOsSharedUsbDevicesPageReady() {
    return chrome.send('notifyGuestOsSharedUsbDevicesPageReady');
  }

  /** @override */
  setGuestOsUsbDeviceShared(vmName, containerName, guid, shared) {
    return chrome.send(
        'setGuestOsUsbDeviceShared', [vmName, containerName, guid, shared]);
  }
}
