// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @typedef {{
 *   fontList: !Array<{
 *       0: string,
 *       1: (string|undefined),
 *       2: (string|undefined)}>,
 * }}
 */
export let FontsData;

/** @interface */
export class FontsBrowserProxy {
  /**
   * @return {!Promise<!FontsData>} Fonts
   */
  fetchFontsData() {}
}

/**
 * @implements {FontsBrowserProxy}
 */
export class FontsBrowserProxyImpl {
  /** @override */
  fetchFontsData() {
    return sendWithPromise('fetchFontsData');
  }
}

addSingletonGetter(FontsBrowserProxyImpl);
