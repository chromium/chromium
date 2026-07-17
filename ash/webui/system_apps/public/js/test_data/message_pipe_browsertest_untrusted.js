// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MessagePipe} from '../message_pipe.js';

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe =
    new MessagePipe('chrome://system-app-test', window.parent);

/**
 * Tells the test driver the guest test message handlers are installed. This
 * requires the test handler that receives the signal to be set up. The order
 * that this occurs can not be guaranteed. So this function retries until the
 * signal is handled, which requires the 'test-handlers-ready' handler to be
 * registered in message_pipe_browsertest_trusted.js.
 */
async function signalTestHandlersReady() {
  const EXPECTED_ERROR =
      `No handler registered for message type 'test-handlers-ready'`;
  while (true) {
    try {
      await parentMessagePipe.sendMessage('test-handlers-ready', {});
      return;
    } catch (/** @type {{message: string}} */ e) {
      if (e.message !== EXPECTED_ERROR) {
        console.error('Unexpected error in signalTestHandlersReady', e);
        return;
      }
    }
  }
}

/** Installs the MessagePipe handlers for receiving test queries. */
function installTestHandlers() {
  // Turn off error rethrowing for tests so the test runner doesn't mark
  // our error handling tests as failed.
  parentMessagePipe.rethrowErrors = false;

  // Log errors, rather than send them to console.error. This allows the error
  // handling tests to work correctly.
  parentMessagePipe.logClientError = error =>
      console.log(JSON.stringify(error));

  parentMessagePipe.registerHandler('success-message', (message) => {
    return {'success': true, 'request': message};
  });

  parentMessagePipe.registerHandler('bad-handler', () => {
    throw Error('This is an error from untrusted');
  });

  parentMessagePipe.registerHandler('request-bad-handler', async () => {
    return parentMessagePipe.sendMessage('bad-handler');
  });

  parentMessagePipe.registerHandler('install-generic-responder', () => {
    // A general postMessage response to all message events. Does not use
    // message pipe.
    window.addEventListener('message', () => {
      window.parent.postMessage('test-response', '*');
    }, false);
  });

  signalTestHandlersReady();
}

// Ensure content and all scripts have loaded before installing test handlers.
if (document.readyState !== 'complete') {
  window.addEventListener('load', installTestHandlers);
} else {
  installTestHandlers();
}
