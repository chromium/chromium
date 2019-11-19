// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * @enum {number}
 * These values must be kept in sync with the values in
 * third_party/cros_system_api/dbus/service_constants.h.
 */
settings.FingerprintResultType = {
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
 *   result: settings.FingerprintResultType,
 *   indexes: !Array<number>,
 * }}
 */
settings.FingerprintAttempt;

/**
 * An object describing a scan from the fingerprint hardware. The structure of
 * this data must be kept in sync with C++ FingerprintHandler.
 * @typedef {{
 *   result: settings.FingerprintResultType,
 *   isComplete: boolean,
 *   percentComplete: number,
 * }}
 */
settings.FingerprintScan;

/**
 * An object describing the necessary info to display on the fingerprint
 * settings. The structure of this data must be kept in sync with
 * C++ FingerprintHandler.
 * @typedef {{
 *   fingerprintsList: !Array<string>,
 *   isMaxed: boolean,
 * }}
 */
settings.FingerprintInfo;

cr.define('settings', function() {
  /** @interface */
  class FingerprintBrowserProxy {
    /**
     * @return {!Promise<!settings.FingerprintInfo>}
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
    removeEnrollment(index) {}

    /**
     * @param {number} index
     * @param {string} newLabel
     * @return {!Promise<boolean>}
     */
    changeEnrollmentLabel(index, newLabel) {}

    startAuthentication() {}
    endCurrentAuthentication() {}

    /**
     * TODO(sammiequon): Temporary function to let the handler know when a
     * completed scan has been sent via click on the setup fingerprint dialog.
     * Remove this when real scans are implemented.
     */
    fakeScanComplete() {}
  }

  /**
   * @implements {settings.FingerprintBrowserProxy}
   */
  class FingerprintBrowserProxyImpl {
    /** @override */
    getFingerprintsList() {
      return cr.sendWithPromise('getFingerprintsList');
    }

    /** @override */
    getNumFingerprints() {
      return cr.sendWithPromise('getNumFingerprints');
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
      return cr.sendWithPromise('getEnrollmentLabel');
    }

    /** @override */
    removeEnrollment(index) {
      return cr.sendWithPromise('removeEnrollment', index);
    }

    /** @override */
    changeEnrollmentLabel(index, newLabel) {
      return cr.sendWithPromise('changeEnrollmentLabel', index, newLabel);
    }

    /** @override */
    startAuthentication() {
      chrome.send('startAuthentication');
    }

    /** @override */
    endCurrentAuthentication() {
      chrome.send('endCurrentAuthentication');
    }

    /** @override */
    fakeScanComplete() {
      chrome.send('fakeScanComplete');
    }
  }

  cr.addSingletonGetter(FingerprintBrowserProxyImpl);

  return {
    FingerprintBrowserProxy: FingerprintBrowserProxy,
    FingerprintBrowserProxyImpl: FingerprintBrowserProxyImpl,
  };
});
