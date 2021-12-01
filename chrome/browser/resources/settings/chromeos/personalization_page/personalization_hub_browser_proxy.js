// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
export class PersonalizationHubBrowserProxy {
  openPersonalizationHub() {}
}

/**
 * @implements {PersonalizationHubBrowserProxy}
 */
export class PersonalizationHubBrowserProxyImpl {
  /** @override */
  openPersonalizationHub() {
    chrome.send('openPersonalizationHub');
  }
}

addSingletonGetter(PersonalizationHubBrowserProxyImpl);
