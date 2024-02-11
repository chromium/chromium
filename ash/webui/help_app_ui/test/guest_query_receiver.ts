// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestMessageResponseData, TestMessageRunTestCase} from './driver_api.js';
import {TEST_ONLY} from './receiver.js';

const {parentMessagePipe} = TEST_ONLY;

interface GenericErrorResponse {
  name: string;
  message: string;
  stack: string;
}

/** Test cases registered by GUEST_TEST. */
const guestTestCases = new Map<string, () => unknown>();

/** Acts on TestMessageRunTestCase. */
async function runTestCase(data: TestMessageRunTestCase):
  Promise<TestMessageResponseData> {
  const testCase = guestTestCases.get(data.testCase);
  if (!testCase) {
    throw new Error(`Unknown test case: '${data.testCase}'`);
  }
  await testCase();  // Propagate exceptions to the MessagePipe handler.
  return {testQueryResult: 'success'};
}

/**
 * Registers a test that runs in the guest context. To indicate failure, the
 * test throws an exception (e.g. via assertEquals).
 */
// eslint-disable-next-line @typescript-eslint/naming-convention
export function GUEST_TEST(testName: string, testCase: () => unknown) {
  guestTestCases.set(testName, testCase);
}

/**
 * Tells the test driver the guest test message handlers are installed. This
 * requires the test handler that receives the signal to be set up. The order
 * that this occurs can not be guaranteed. So this function retries until the
 * signal is handled, which requires the 'test-handlers-ready' handler to be
 * registered in driver.js.
 */
async function signalTestHandlersReady() {
  const EXPECTED_ERROR =
      /No handler registered for message type 'test-handlers-ready'/;
  // Attempt to signal to the driver that we are ready to run tests, give up
  // after 10 tries and assume something went wrong so we don't spam the error
  // log too much.
  let attempts = 10;
  while (--attempts >= 0) {
    try {
      // Try to limit log output from message pipe errors.
      await new Promise(resolve => setTimeout(resolve, 100));
      await parentMessagePipe.sendMessage('test-handlers-ready', {});
      return;
    } catch (error: unknown) {
      const e = error as GenericErrorResponse;
      if (!EXPECTED_ERROR.test(e.message)) {
        console.error('Unexpected error in signalTestHandlersReady', e);
        return;
      }
    }
  }
  console.error('signalTestHandlersReady failed to signal.');
}

/** Installs the MessagePipe handlers for receiving test queries. */
function installTestHandlers() {
  // Turn off error rethrowing for tests so the test runner doesn't mark
  // our error handling tests as failed.
  parentMessagePipe.rethrowErrors = false;

  parentMessagePipe.registerHandler(
    'run-test-case', (data: TestMessageRunTestCase) => {
    return runTestCase(data);
  });

  // Log errors, rather than send them to console.error. This allows the error
  // handling tests to work correctly, and is also required for
  // signalTestHandlersReady() to operate without failing tests.
  parentMessagePipe.logClientError = (error: unknown) =>
      console.log(JSON.stringify(error));

  signalTestHandlersReady();
}

// Ensure content and all scripts have loaded before installing test handlers.
if (document.readyState !== 'complete') {
  window.addEventListener('load', installTestHandlers);
} else {
  installTestHandlers();
}
