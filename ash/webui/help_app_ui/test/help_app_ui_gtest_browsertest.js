// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview js2gtest wrapper for the chrome://help-app test suite. Actual
 * test implementations live in help_app_ui_browsertest.js and
 * help_app_guest_ui_browsertest.js.
 */
GEN('#include "ash/webui/help_app_ui/test/help_app_ui_browsertest.h"');

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "ash/public/cpp/style/dark_light_mode_controller.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://help-app';

// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var HelpAppUIGtestBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kHelpAppLauncherSearch',
      ],
    };
  }

  /** @override */
  get typedefCppFixture() {
    return 'HelpAppUiBrowserTest';
  }
};

// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var HelpAppUIWithDarkModeGtestBrowserTest =
    class extends HelpAppUIGtestBrowserTest {
  /** @override */
  get testGenPreamble() {
    return () => {
      // Switch to dark mode.
      GEN('ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);');
    };
  }
};

async function GetTestHarness() {
  const testHarnessPolicy = trustedTypes.createPolicy('test-harness', {
    createScriptURL: () => './help_app_ui_browsertest.js',
  });

  const tests =
      /** @type {!HTMLScriptElement} */ (document.createElement('script'));
  tests.src = testHarnessPolicy.createScriptURL('');
  tests.type = 'module';
  await new Promise((resolve, reject) => {
    tests.onload = resolve;
    tests.onerror = reject;
    document.body.appendChild(tests);
  });

  // When help_app_ui_browsertest.js loads, it will add this onto the window.
  return window['HelpAppUIBrowserTest_for_js2gtest'];
}

/**
 * Small helper to run the `name` test function on the `HelpAppUIBrowserTest`
 * object we get from `help_app_ui_browsertest.js`.
 */
async function runHelpAppTest(name, guest = false) {
  const HelpAppUIBrowserTest = await GetTestHarness();
  try {
    if (guest) {
      await HelpAppUIBrowserTest.runTestInGuest(name);
    } else {
      await HelpAppUIBrowserTest[name]();
    }
    testDone();
  } catch (/* @type {Error} */ error) {
    let message = 'exception';
    if (typeof error === 'object' && error !== null && error['message']) {
      message = error['message'];
      console.log(error['stack']);
    } else {
      console.log(error);
    }
    /** @type {function(*)} */ (testDone)([false, message]);
  }
}

function runHelpAppTestInGuest(name) {
  return runHelpAppTest(name, true);
}

// Ensure every test body has a `TEST_F` call in this file.
TEST_F('HelpAppUIGtestBrowserTest', 'ConsistencyCheck', async () => {
  const HelpAppUIBrowserTest = await GetTestHarness();
  const bodies = {
    ...(/** @type {{testCaseBodies: Object}} */ (HelpAppUIGtestBrowserTest))
        .testCaseBodies,
    ...(/** @type {{testCaseBodies: Object}} */ (
            HelpAppUIWithDarkModeGtestBrowserTest))
        .testCaseBodies,
  };
  for (const f in HelpAppUIBrowserTest) {
    if (f === 'runTestInGuest') {
      continue;
    }
    if (!(f in bodies || `DISABLED_${f}` in bodies)) {
      console.error(
          `HelpAppUIBrowserTest.${f} is missing a TEST_F and will not be run.`);
    }
  }
  testDone();
});

TEST_F('HelpAppUIGtestBrowserTest', 'HasChromeSchemeURL', () => {
  runHelpAppTest('HasChromeSchemeURL');
});

TEST_F('HelpAppUIGtestBrowserTest', 'HasTitleAndLang', () => {
  runHelpAppTest('HasTitleAndLang');
});

TEST_F(
    'HelpAppUIWithDarkModeGtestBrowserTest',
    'BodyHasCorrectBackgroundColorInDarkMode', () => {
      runHelpAppTest('BodyHasCorrectBackgroundColorInDarkMode');
    });

// Test cases injected into the guest context.
// See implementations in `help_app_guest_ui_browsertest.js`.

TEST_F('HelpAppUIGtestBrowserTest', 'GuestHasLang', () => {
  runHelpAppTestInGuest('GuestHasLang');
});

TEST_F('HelpAppUIGtestBrowserTest', 'GuestLoadsLoadTimeData', () => {
  runHelpAppTestInGuest('GuestLoadsLoadTimeData');
});

TEST_F('HelpAppUIGtestBrowserTest', 'GuestCanSearchWithHeadings', () => {
  runHelpAppTestInGuest('GuestCanSearchWithHeadings');
});

TEST_F('HelpAppUIGtestBrowserTest', 'GuestCanSearchWithCategories', () => {
  runHelpAppTestInGuest('GuestCanSearchWithCategories');
});

TEST_F('HelpAppUIGtestBrowserTest', 'GuestCanClearSearchIndex', () => {
  runHelpAppTestInGuest('GuestCanClearSearchIndex');
});

TEST_F('HelpAppUIGtestBrowserTest', 'GuestCanGetDeviceInfo', () => {
  runHelpAppTestInGuest('GuestCanGetDeviceInfo');
});
