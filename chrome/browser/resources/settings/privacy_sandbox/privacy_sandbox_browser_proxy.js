// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * Information about the user's current FLoC cohort identifier.
 * @typedef {{trialStatus: string,
 *            cohort: string,
 *            nextUpdate: string,
 *            canReset: boolean}}
 */
export let FlocIdentifier;

/** @interface */
export class PrivacySandboxBrowserProxy {
  /**
   * Gets the user's current FLoC cohort identifier information.
   * @return {!Promise<!FlocIdentifier>}
   */
  getFlocId() {}

  /** Resets the user's FLoC cohort identifier. */
  resetFlocId() {}
}

/** @implements {PrivacySandboxBrowserProxy} */
export class PrivacySandboxBrowserProxyImpl {
  /** @override */
  getFlocId() {
    return sendWithPromise('getFlocId');
  }

  /** @override */
  resetFlocId() {
    chrome.send('resetFlocId');
  }
}

addSingletonGetter(PrivacySandboxBrowserProxyImpl);
