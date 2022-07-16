// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test cases registered by UNTRUSTED_TEST.
 * @type {Map<string, function(): Promise<undefined>>}
 */
const untrustedTestCases = new Map();

/**
 * @param {string} testName
 * @return {!Promise<string>}
 */
async function runTestCase(testName) {
  const testCase = untrustedTestCases.get(testName);
  if (!testCase) {
    throw new Error(`Unknown test case: '${testName}'`);
  }
  await testCase();  // Propagate exceptions to the MessagePipe handler.
  return 'success';
}

/**
 * Registers a test that runs in the untrusted context. To indicate failure, the
 * test throws an exception (e.g. via assertEquals).
 * @param {string} testName
 * @param {function(): Promise<undefined>} testCase
 */
function UNTRUSTED_TEST(testName, testCase) {
  untrustedTestCases.set(testName, testCase);
}

function registerTestHandlers() {
  dpsl.internal.messagePipe.registerHandler(
      'run-test-case', (message) => {
        const {testName} = /** @type {{testName: !string}} */ (message);
        return runTestCase(testName);
      });
}

registerTestHandlers();
