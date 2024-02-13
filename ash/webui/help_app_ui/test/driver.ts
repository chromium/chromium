// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(b/169279800): Pull out test code that media app and help app have in
// common.

import {TEST_ONLY} from './browser_proxy.js';
import type {TestMessageRunTestCase} from './driver_api.js';

const {guestMessagePipe} = TEST_ONLY;

/**
 * Promise that signals the guest is ready to receive test messages (in addition
 * to messages handled by receiver.js).
 */
const testMessageHandlersReady = new Promise(resolve => {
  guestMessagePipe.registerHandler('test-handlers-ready', resolve);
});

/** Runs the given `testCase` in the guest context. */
export async function runTestInGuest(testCase: string) {
  const message: TestMessageRunTestCase = {testCase};
  await testMessageHandlersReady;
  await guestMessagePipe.sendMessage('run-test-case', message);
}
