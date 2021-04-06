// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the profilecustomization bubble to
 * interact with the browser.
 */

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * Profile info (colors and avatar) sent from C++.
 * @typedef {{
 *   backgroundColor: string,
 *   pictureUrl: string,
 *   isManaged: boolean,
 *   welcomeTitle: string,
 * }}
 */
export let ProfileInfo;

/** @interface */
export class ProfileCustomizationBrowserProxy {
  /**
   * Called when the page is ready
   * @return {!Promise<!ProfileInfo>}
   */
  initialized() {}

  /**
   * Called when the user clicks the done button.
   * @param {string} profileName
   */
  done(profileName) {}
}

/** @implements {ProfileCustomizationBrowserProxy} */
export class ProfileCustomizationBrowserProxyImpl {
  /** @override */
  initialized() {
    return sendWithPromise('initialized');
  }

  /** @override */
  done(profileName) {
    chrome.send('done', [profileName]);
  }
}

addSingletonGetter(ProfileCustomizationBrowserProxyImpl);
