// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "add SMB share" dialog to
 * interact with the browser. Used only on Chrome OS.
 */

import {sendWithPromise} from '//resources/ash/common/cr.m.js';

/**
 *  @enum {number}
 *  These values must be kept in sync with the SmbMountResult enum in
 *  chrome/browser/ash/smb_client/smb_errors.h.
 */
export const SmbMountResult = {
  SUCCESS: 0,
  UNKNOWN_FAILURE: 1,
  AUTHENTICATION_FAILED: 2,
  NOT_FOUND: 3,
  UNSUPPORTED_DEVICE: 4,
  MOUNT_EXISTS: 5,
  INVALID_URL: 6,
  INVALID_OPERATION: 7,
  DBUS_PARSE_FAILED: 8,
  OUT_OF_MEMORY: 9,
  ABORTED: 10,
  IO_ERROR: 11,
  TOO_MANY_OPENED: 12,
  INVALID_SSO_URL: 13,
  INVALID_USERNAME: 14,
};

/** @enum {string} */
export const SmbAuthMethod = {
  KERBEROS: 'kerberos',
  CREDENTIALS: 'credentials',
};

/** @type {SmbBrowserProxy|null} */
let instance = null;

/** @interface */
export class SmbBrowserProxy {
  /**
   * Attempts to mount an Smb filesystem with the provided url.
   * @param {string} smbUrl File Share URL.
   * @param {string} smbName Display name for the File Share.
   * @param {string} username
   * @param {string} password
   * @param {string} authMethod
   * @param {boolean} shouldOpenFileManagerAfterMount
   * @param {boolean} saveCredentials
   * @return {!Promise<SmbMountResult>}
   */
  smbMount(
      smbUrl, smbName, username, password, authMethod,
      shouldOpenFileManagerAfterMount, saveCredentials) {}

  /**
   * Starts the file share discovery process.
   */
  startDiscovery() {}

  /**
   * Updates the credentials for a mounted share.
   * @param {string} mountId
   * @param {string} username
   * @param {string} password
   */
  updateCredentials(mountId, username, password) {}

  /**
   * Returns true if any SMB has been configured or saved before. Called when
   * the settings page initially loads.
   * @returns {Promise<boolean>}
   */
  hasAnySmbMountedBefore() {}
}

/** @implements {SmbBrowserProxy} */
export class SmbBrowserProxyImpl {
  /** @return {!SmbBrowserProxy} */
  static getInstance() {
    return instance || (instance = new SmbBrowserProxyImpl());
  }

  /** @param {!SmbBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }

  /** @override */
  smbMount(
      smbUrl, smbName, username, password, authMethod,
      shouldOpenFileManagerAfterMount, saveCredentials) {
    return sendWithPromise(
        'smbMount', smbUrl, smbName, username, password,
        authMethod === SmbAuthMethod.KERBEROS, shouldOpenFileManagerAfterMount,
        saveCredentials);
  }

  startDiscovery() {
    chrome.send('startDiscovery');
  }

  updateCredentials(mountId, username, password) {
    chrome.send('updateCredentials', [mountId, username, password]);
  }

  hasAnySmbMountedBefore() {
    return sendWithPromise('hasAnySmbMountedBefore');
  }
}
