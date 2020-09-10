// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
/* #export */ class OsA11yPageBrowserProxy {
  /**
   * Requests whether screen reader state changed. Result
   * is returned by the 'screen-reader-state-changed' WebUI listener event.
   */
  a11yPageReady() {}

  /**
   * Opens the a11y image labels modal dialog.
   */
  confirmA11yImageLabels() {}
}

/**
 * @implements {OsA11yPageBrowserProxy}
 */
/* #export */ class OsA11yPageBrowserProxyImpl {
  /** @override */
  a11yPageReady() {
    chrome.send('a11yPageReady');
  }

  /** @override */
  confirmA11yImageLabels() {
    chrome.send('confirmA11yImageLabels');
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
cr.addSingletonGetter(OsA11yPageBrowserProxyImpl);