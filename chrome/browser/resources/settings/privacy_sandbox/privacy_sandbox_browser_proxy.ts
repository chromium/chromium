// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * Information about the user's current FLoC cohort identifier.
 */
export type FlocIdentifier = {
  trialStatus: string,
  cohort: string,
  nextUpdate: string,
  canReset: boolean,
};

export interface PrivacySandboxBrowserProxy {
  /**
   * Gets the user's current FLoC cohort identifier information.
   */
  getFlocId(): Promise<FlocIdentifier>;

  /** Resets the user's FLoC cohort identifier. */
  resetFlocId(): void;
}

export class PrivacySandboxBrowserProxyImpl implements
    PrivacySandboxBrowserProxy {
  getFlocId() {
    return sendWithPromise('getFlocId');
  }

  resetFlocId() {
    chrome.send('resetFlocId');
  }

  static getInstance(): PrivacySandboxBrowserProxy {
    return instance || (instance = new PrivacySandboxBrowserProxyImpl());
  }

  static setInstance(obj: PrivacySandboxBrowserProxy) {
    instance = obj;
  }
}

let instance: PrivacySandboxBrowserProxy|null = null;
