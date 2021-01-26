// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the work profile confirmation dialog to
 * interact with the browser.
 */
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class WorkProfileConfirmationBrowserProxy {
  /**
   * Called when the user confirms and creates a work profile.
   */
  confirm() {}

  /**
   * Called when the user cancel the creation of a work profile.
   */
  cancel() {}

  /** @param {!Array<number>} height */
  initializedWithSize(height) {}

  /**
   * Called when the WebUIListener for "account-image-changed" was added.
   */
  requestAccountImage() {}
}

/** @implements {WorkProfileConfirmationBrowserProxy} */
export class WorkProfileConfirmationBrowserProxyImpl {
  /** @override */
  confirm() {
    chrome.send('confirm');
  }

  /** @override */
  cancel() {
    chrome.send('cancel');
  }

  /** @override */
  initializedWithSize(height) {
    chrome.send('initializedWithSize', height);
  }

  /** @override */
  requestAccountImage() {
    chrome.send('accountImageRequest');
  }
}

addSingletonGetter(WorkProfileConfirmationBrowserProxyImpl);
