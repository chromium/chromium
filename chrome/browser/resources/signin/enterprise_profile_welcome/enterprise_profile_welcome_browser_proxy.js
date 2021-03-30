// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the enterprise profile welcome screen
 * to interact with the browser.
 */

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * Enterprise profile info sent from C++.
 * @typedef {{
 *   backgroundColor: string,
 *   pictureUrl: string,
 *   showEnterpriseBadge: boolean,
 *   enterpriseTitle: string,
 *   enterpriseInfo: string,
 *   proceedLabel: string,
 * }}
 */
export let EnterpriseProfileInfo;

/** @interface */
export class EnterpriseProfileWelcomeBrowserProxy {
  /**
   * Called when the page is ready
   * @return {!Promise<!EnterpriseProfileInfo>}
   */
  initialized() {}

  /**
   * Called when the user clicks the proceed button.
   */
  proceed() {}

  /**
   * Called when the user clicks the cancel button.
   */
  cancel() {}
}

/** @implements {EnterpriseProfileWelcomeBrowserProxy} */
export class EnterpriseProfileWelcomeBrowserProxyImpl {
  /** @override */
  initialized() {
    return sendWithPromise('initialized');
  }

  /** @override */
  proceed() {
    chrome.send('proceed');
  }

  /** @override */
  cancel() {
    chrome.send('cancel');
  }
}

addSingletonGetter(EnterpriseProfileWelcomeBrowserProxyImpl);