// Copyright 2024 The Chromium Authors
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

const HOST_ORIGIN = 'chrome://help-app';
const GUEST_ORIGIN = 'chrome-untrusted://help-app';

interface TestSuite {
  [testName: string]: () => unknown;
  runTestInGuest: (testName?: string) => unknown;
}

const HelpAppUIBrowserTest: TestSuite = {
  /**
   * Expose the runTestInGuest function to help_app_ui_gtest_browsertest.js so
   * it can call it.
   * runTestInGuest takes a compulsory string arg, which isn't compatible with
   * the TestSuite index signature, so cast it here.
   */
  runTestInGuest: runTestInGuest as () => unknown,
};

// Expose an export for tests run through `isolatedTestRunner`.
(window as unknown as {HelpAppUiBrowserTest: {}})['HelpAppUiBrowserTest'] =
  HelpAppUIBrowserTest;

// Tests that chrome://help-app goes somewhere instead of 404ing or crashing.
HelpAppUIBrowserTest['HasChromeSchemeURL'] = async () => {
  const {assertEquals} = await import('//webui-test/chai_assert.js');
  const guest =
      /** @type {!HTMLIFrameElement} */ (document.querySelector('iframe'));

  assertEquals(document.location.origin, HOST_ORIGIN);
  assertEquals(guest!.src, GUEST_ORIGIN + '/');
};

// Tests that we have localized information in the HTML like title and lang.
HelpAppUIBrowserTest['HasTitleAndLang'] = async () => {
  const {assertEquals} = await import('//webui-test/chai_assert.js');
  assertEquals(document.documentElement.lang, 'en');
  assertEquals(document.title, 'Explore');
};

// Check the body element's background color when the dark mode is enabled.
HelpAppUIBrowserTest['BodyHasCorrectBackgroundColorInDarkMode'] = async () => {
  const {assertEquals} = await import('//webui-test/chai_assert.js');
  const actualBackgroundColor = getComputedStyle(document.body).backgroundColor;
  assertEquals(actualBackgroundColor, 'rgb(32, 33, 36)');  // Grey 900.
};
