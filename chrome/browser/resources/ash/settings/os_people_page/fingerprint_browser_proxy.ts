// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * These values must be kept in sync with the values in
 * third_party/cros_system_api/dbus/service_constants.h.
 */
export enum FingerprintResultType {
  SUCCESS = 0,
  PARTIAL = 1,
  INSUFFICIENT = 2,
  SENSOR_DIRTY = 3,
  TOO_SLOW = 4,
  TOO_FAST = 5,
  IMMOBILE = 6,
}

/**
 * An object describing a attempt from the fingerprint hardware. The structure
 * of this data must be kept in sync with C++ FingerprintHandler.
 */
export interface FingerprintAttempt {
  result: FingerprintResultType;
  indexes: number[];
}

/**
 * An object describing a scan from the fingerprint hardware. The structure of
 * this data must be kept in sync with C++ FingerprintHandler.
 */
export interface FingerprintScan {
  result: FingerprintResultType;
  isComplete: boolean;
  percentComplete: number;
}

/**
 * An object describing the necessary info to display on the fingerprint
 * settings. The structure of this data must be kept in sync with
 * C++ FingerprintHandler.
 */
export interface FingerprintInfo {
  fingerprintsList: string[];
  isMaxed: boolean;
}

export interface FingerprintBrowserProxy {
  getFingerprintsList(): Promise<FingerprintInfo>;

  getNumFingerprints(): Promise<number>;

  startEnroll(authToken: string): void;

  cancelCurrentEnroll(): void;

  getEnrollmentLabel(index: number): Promise<string>;

  removeEnrollment(index: number, authToken: string): Promise<boolean>;

  changeEnrollmentLabel(index: number, newLabel: string): Promise<boolean>;

  /**
   * TODO(sammiequon): Temporary function to let the handler know when a
   * completed scan has been sent via click on the setup fingerprint dialog.
   * Remove this when real scans are implemented.
   */
  fakeScanComplete(): void;
}

let instance: FingerprintBrowserProxy|null = null;

export class FingerprintBrowserProxyImpl implements FingerprintBrowserProxy {
  static getInstance(): FingerprintBrowserProxy {
    return instance || (instance = new FingerprintBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: FingerprintBrowserProxy): void {
    instance = obj;
  }

  getFingerprintsList(): Promise<FingerprintInfo> {
    return sendWithPromise('getFingerprintsList');
  }

  getNumFingerprints(): Promise<number> {
    return sendWithPromise('getNumFingerprints');
  }

  startEnroll(authToken: string): void {
    chrome.send('startEnroll', [authToken]);
  }

  cancelCurrentEnroll(): void {
    chrome.send('cancelCurrentEnroll');
  }

  getEnrollmentLabel(_index: number): Promise<string> {
    return sendWithPromise('getEnrollmentLabel');
  }

  removeEnrollment(index: number, authToken: string): Promise<boolean> {
    return sendWithPromise('removeEnrollment', index, authToken);
  }

  changeEnrollmentLabel(index: number, newLabel: string): Promise<boolean> {
    return sendWithPromise('changeEnrollmentLabel', index, newLabel);
  }

  fakeScanComplete(): void {
    chrome.send('fakeScanComplete');
  }
}