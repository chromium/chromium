// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the profilecustomization bubble to
 * interact with the browser.
 */

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class ProfileCustomizationBrowserProxy {
  /** Called when the user clicks the done button. */
  done() {}
}

/** @implements {ProfileCustomizationBrowserProxy} */
export class ProfileCustomizationBrowserProxyImpl {
  /** @override */
  done() {
    chrome.send('done');
  }
}

addSingletonGetter(ProfileCustomizationBrowserProxyImpl);
