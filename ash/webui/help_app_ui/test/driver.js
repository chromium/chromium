// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(b/169279800): Pull out test code that media app and help app have in
// common.

// Note we can only import from 'browser_proxy.js': other modules are rolled-up
// into it, and already loaded.
import {TEST_ONLY} from './browser_proxy.js';
const {guestMessagePipe} = TEST_ONLY;

/**
 * Promise that signals the guest is ready to receive test messages (in addition
 * to messages handled by receiver.js).
 * @type {!Promise<undefined>}
 */
const testMessageHandlersReady = new Promise(resolve => {
  guestMessagePipe.registerHandler('test-handlers-ready', resolve);
});

/**
 * Runs the given `testCase` in the guest context.
 * @param {string} testCase
 */
export async function runTestInGuest(testCase) {
  /** @type {!TestMessageRunTestCase} */
  const message = {testCase};
  await testMessageHandlersReady;
  await guestMessagePipe.sendMessage('run-test-case', message);
}
