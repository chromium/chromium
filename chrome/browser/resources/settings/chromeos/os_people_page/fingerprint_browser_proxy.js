// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

/**
 * @enum {number}
 * These values must be kept in sync with the values in
 * third_party/cros_system_api/dbus/service_constants.h.
 */
export const FingerprintResultType = {
  SUCCESS: 0,
  PARTIAL: 1,
  INSUFFICIENT: 2,
  SENSOR_DIRTY: 3,
  TOO_SLOW: 4,
  TOO_FAST: 5,
  IMMOBILE: 6,
};

/**
 * An object describing a attempt from the fingerprint hardware. The structure
 * of this data must be kept in sync with C++ FingerprintHandler.
 * @typedef {{
 *   result: FingerprintResultType,
 *   indexes: !Array<number>,
 * }}
 */
export let FingerprintAttempt;

/**
 * An object describing a scan from the fingerprint hardware. The structure of
 * this data must be kept in sync with C++ FingerprintHandler.
 * @typedef {{
 *   result: FingerprintResultType,
 *   isComplete: boolean,
 *   percentComplete: number,
 * }}
 */
export let FingerprintScan;

/**
 * An object describing the necessary info to display on the fingerprint
 * settings. The structure of this data must be kept in sync with
 * C++ FingerprintHandler.
 * @typedef {{
 *   fingerprintsList: !Array<string>,
 *   isMaxed: boolean,
 * }}
 */
export let FingerprintInfo;

/** @interface */
export class FingerprintBrowserProxy {
  /**
   * @return {!Promise<!FingerprintInfo>}
   */
  getFingerprintsList() {}

  /**
   * @return {!Promise<number>}
   */
  getNumFingerprints() {}

  /**
   * @param {string} authToken
   */
  startEnroll(authToken) {}

  cancelCurrentEnroll() {}

  /**
   * @param {number} index
   * @return {!Promise<string>}
   */
  getEnrollmentLabel(index) {}

  /**
   * @param {number} index
   * @return {!Promise<boolean>}
   */
  removeEnrollment(index, authToken) {}

  /**
   * @param {number} index
   * @param {string} newLabel
   * @return {!Promise<boolean>}
   */
  changeEnrollmentLabel(index, newLabel) {}

  /**
   * TODO(sammiequon): Temporary function to let the handler know when a
   * completed scan has been sent via click on the setup fingerprint dialog.
   * Remove this when real scans are implemented.
   */
  fakeScanComplete() {}
}

/** @type {?FingerprintBrowserProxy} */
let instance = null;

/**
 * @implements {FingerprintBrowserProxy}
 */
export class FingerprintBrowserProxyImpl {
  /** @return {!FingerprintBrowserProxy} */
  static getInstance() {
    return instance || (instance = new FingerprintBrowserProxyImpl());
  }

  /** @param {!FingerprintBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  getFingerprintsList() {
    return sendWithPromise('getFingerprintsList');
  }

  /** @override */
  getNumFingerprints() {
    return sendWithPromise('getNumFingerprints');
  }

  /** @override */
  startEnroll(authToken) {
    chrome.send('startEnroll', [authToken]);
  }

  /** @override */
  cancelCurrentEnroll() {
    chrome.send('cancelCurrentEnroll');
  }

  /** @override */
  getEnrollmentLabel(index) {
    return sendWithPromise('getEnrollmentLabel');
  }

  /** @override */
  removeEnrollment(index, authToken) {
    return sendWithPromise('removeEnrollment', index, authToken);
  }

  /** @override */
  changeEnrollmentLabel(index, newLabel) {
    return sendWithPromise('changeEnrollmentLabel', index, newLabel);
  }

  /** @override */
  fakeScanComplete() {
    chrome.send('fakeScanComplete');
  }
}
