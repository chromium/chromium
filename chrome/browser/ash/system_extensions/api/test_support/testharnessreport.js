// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * This file implements code to integrate browser tests with testharness.js
 *
 * It attaches a completion callback that notifies the browser that all tests
 * have completed and their status.
 *
 * For more documentation about the callback functions and the
 * parameters they are called with, see testharness.js, or the docs at:
 * https://web-platform-tests.org/writing-tests/testharness-api.html
 */
(() => {
  const TestHarnessResult = systemExtensionsTest.mojom.TestHarnessResult;
  const TestHarnessStatus = systemExtensionsTest.mojom.TestHarnessStatus;
  const TestResult = systemExtensionsTest.mojom.TestResult;
  const TestStatus = systemExtensionsTest.mojom.TestStatus;

  globalThis.testRunner = new systemExtensionsTest.mojom.TestRunnerRemote;
  globalThis.testRunner.$.bindNewPipeAndPassReceiver().bindInBrowser('process');

  // Simulating an event before the service worker is installed, fails because
  // the service worker is not considered registered yet. We have to wait for
  // the service worker to be fully registered before we try to simulate events.
  const activatePromise = new Promise(resolve => {
    let wrapper = function(event) {
      globalThis.removeEventListener('activate', wrapper);
      resolve(event);
    };
    globalThis.addEventListener('activate', wrapper);
  });

  async function async_setup() {
    if (registration.active) {
      // In the first run, testharness.js uses the 'install' event to know when
      // all tests have been registered.
      //
      // In subsequent runs, testharness.js waits for a `message` event to know
      // when all tests have been resgistered. On the Web platform, this event
      // is sent from a page. System extensions fake this event because they
      // don't have an associated page.
      //
      // Use setTimeout so that the event is fired after tests have been
      // registered.
      setTimeout(() => globalThis.dispatchEvent(new Event('message')), 0);

      // If the service worker is already active, then no need to wait for the
      // 'activate' event.
      return;
    }

    await activatePromise;
  }

  promise_setup(async_setup, {
    // The default output formats test results into an HTML table, but for
    // the System Extensions, we output the test results to the console.
    'output': false,
    // Chromium browser tests have their own timeout.
    'explicit_timeout': true
  });

  // Converts the status returned by the testharness API into a
  // system_extensions_test.mojom.TestStatus. Needed because the names are not
  // exposed by testharness.js. These values are defined internally in
  // testharness.js as Test.statuses.
  function convertTestStatus(testStatus) {
    switch (testStatus) {
      case 0:
        return TestStatus.kPass;
      case 1:
        return TestStatus.kFail;
      case 2:
        return TestStatus.kTimeout;
      case 3:
        return TestStatus.kNotRun;
      case 4:
        return TestStatus.kPreconditionFailed;
    }
    assert_unreached('unrecognized test status');
  }

  // Converts the status returned by the testharness API into a
  // system_extensions_test.mojom.TestharnessStatus. Needed because the names
  // are not exposed by testharness.js. These values are defined internally in
  // testharness.js as TestsStatus.statuses.
  function convertTestHarnessStatus(testharnessStatus) {
    switch (testharnessStatus) {
      case 0:
        return TestHarnessStatus.kOk;
      case 1:
        return TestHarnessStatus.kError;
      case 2:
        return TestHarnessStatus.kTimeout;
      case 3:
        return TestHarnessStatus.kPreconditionFailed;
    }
    assert_unreached('unrecognized test harness status');
  }

  const skippedTests = new Set();
  globalThis.skipTest =
      (testName) => {
        skippedTests.add(testName);
      }

  add_completion_callback((tests, testharnessStatus) => {
    const testResults = []
    for (const test of tests) {
      // Multi-run tests skip tests. In those cases, there
      // is no need to send the result to the browser.
      if (skippedTests.has(test.name)) {
        continue;
      }

      const testResult = new TestResult();
      testResult.name = test.name;
      testResult.message = test.message;
      testResult.stack = test.stack;
      testResult.status = convertTestStatus(test.status);

      testResults.push(testResult);
    }

    const testharnessResult = new TestHarnessResult();
    testharnessResult.message = testharnessStatus.message;
    testharnessResult.stack = testharnessStatus.stack;
    testharnessResult.status = convertTestHarnessStatus(
      testharnessStatus.status);

    testRunner.onCompletion(testResults, testharnessResult);
  });
})();
