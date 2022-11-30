// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://help-app. The tests are actually
 * invoked in help_app_ui_gtest_browsertest.js, this file simply packages up
 * each tests logic into a single object that file can import.
 *
 * To add a new test to this file, add the test function to
 * `HelpAppUIBrowserTest` and then invoke in in gtest_browsertest.js.
 */
import {runTestInGuest} from './driver.js';

const GUEST_ORIGIN = 'chrome-untrusted://help-app';

/** @struct */
const HelpAppUIBrowserTest = {
  /**
   * Expose the runTestInGuest function to help_app_ui_gtest_browsertest.js so
   * it can call it.
   * @type function(string): !Promise<undefined>
   */
  runTestInGuest,
};

// Expose an old-style export for js2gtest.
window['HelpAppUIBrowserTest_for_js2gtest'] = HelpAppUIBrowserTest;

// Tests that chrome://help-app goes somewhere instead of 404ing or crashing.
HelpAppUIBrowserTest.HasChromeSchemeURL = () => {
  const guest =
      /** @type {!HTMLIFrameElement} */ (document.querySelector('iframe'));

  assertEquals(document.location.origin, HOST_ORIGIN);
  assertEquals(guest.src, GUEST_ORIGIN + '/');
};

// Tests that we have localized information in the HTML like title and lang.
HelpAppUIBrowserTest.HasTitleAndLang = () => {
  assertEquals(document.documentElement.lang, 'en');
  assertEquals(document.title, 'Explore');
};

// Check the body element's background color when the DarkLightMode feature is
// enabled and dark mode is enabled.
HelpAppUIBrowserTest.BodyHasCorrectBackgroundColorWithDarkLight = () => {
  const actualBackgroundColor = getComputedStyle(document.body).backgroundColor;
  assertEquals(actualBackgroundColor, 'rgb(32, 33, 36)');  // Grey 900.
};

// Check the body element's background color when the DarkLightMode feature is
// disabled.
HelpAppUIBrowserTest.BodyHasCorrectBackgroundColorWithoutDarkLight = () => {
  const actualBackgroundColor = getComputedStyle(document.body).backgroundColor;
  // The default background-color of <body> is transparent.
  assertEquals(actualBackgroundColor, 'rgba(0, 0, 0, 0)');
};
