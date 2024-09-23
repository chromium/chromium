// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for message_pipe.js.
 */
GEN('#include "ash/webui/web_applications/test/js_library_test.h"');

GEN('#include "content/public/test/browser_test.h"');

/**
 * Wraps `chai.assert.match` allowing tests to use `assertMatch`.
 * @param {string} string the string to match
 * @param {string} regex an escaped regex compatible string
 * @param {string=} opt_message logged if the assertion fails
 */
function assertMatch(string, regex, opt_message = undefined) {
  chai.assert.match(string, new RegExp(regex), opt_message);
}

/**
 * Use to match error stack traces.
 * @param {string} stackTrace the stacktrace
 * @param {!Array<string>} regexLines a list of escaped regex compatible
 *     strings, used to compare with the stacktrace.
 * @param {string=} opt_message logged if the assertion fails
 */
function assertMatchErrorStack(
    stackTrace, regexLines, opt_message = undefined) {
  const regex = `(.|\\n)*${regexLines.join('(.|\\n)*')}(.|\\n)*`;
  assertMatch(stackTrace, regex, opt_message);
}

/**
 * @param {string} messageType
 * @param {!Object=} message
 * @return {!Promise<!Object>}
 */
async function sendTestMessage(messageType, message = {}) {
  await window['testMessageHandlersReady'];
  return window['untrustedMessagePipe'].sendMessage(messageType, message);
}

// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var MessagePipeBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://system-app-test/test_data/message_pipe_browsertest_trusted.html';
  }

  /** @override */
  get typedefCppFixture() {
    return 'JsLibraryTest';
  }

  /** @override */
  get isAsync() {
    return true;
  }
};

TEST_F('MessagePipeBrowserTest', 'ReceivesSuccessResponse', async () => {
  const {assertDeepEquals} = await import('chrome://webui-test/chai_assert.js');
  const request = {'foo': 'bar'};
  const response = await sendTestMessage('success-message', request);
  assertDeepEquals(response, {'success': true, 'request': request});
  testDone();
});

TEST_F('MessagePipeBrowserTest', 'IgnoresMessagesWithNoType', async () => {
  const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
  await sendTestMessage('install-generic-responder');

  let messageCount = 0;
  const receiveMessage = event => {
    messageCount++;
    // There should be one 'response' for each of the postMessages below.
    // There should be no response from parentMessagePipe because it should
    // ignore the messages below.
    assertEquals(event.data, 'test-response');
    if (messageCount === 5) {
      testDone();
    }
  };
  window.addEventListener('message', receiveMessage, false);
  const guestFrame = /** @type {!HTMLIFrameElement} */ (
      document.querySelector('iframe'));
  const TEST_GUEST_ORIGIN = 'chrome-untrusted://system-app-test';
  // These postMessages should be ignored and not cause any errors.
  guestFrame.contentWindow.postMessage('test', TEST_GUEST_ORIGIN);
  guestFrame.contentWindow.postMessage({type: 9}, TEST_GUEST_ORIGIN);
  guestFrame.contentWindow.postMessage({}, TEST_GUEST_ORIGIN);
  guestFrame.contentWindow.postMessage(null, TEST_GUEST_ORIGIN);
  guestFrame.contentWindow.postMessage(undefined, TEST_GUEST_ORIGIN);
});

// Tests that we receive an error if our message is unhandled.
TEST_F('MessagePipeBrowserTest', 'ReceivesNoHandlerError', async () => {
  const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
  window['untrustedMessagePipe'].logClientError = error =>
      console.log(JSON.stringify(error));
  let caughtError = {};

  try {
    await sendTestMessage('unknown-message');
  } catch (error) {
    caughtError = error;
  }

  assertEquals(caughtError.name, 'Error');
  assertEquals(
      caughtError.message,
      'unknown-message: No handler registered for message type \'unknown-message\'');
  assertMatchErrorStack(caughtError.stack, [
    // Error stack of the test context.
    'Error: unknown-message: No handler registered for message type \'unknown-message\'',
    'at MessagePipe.sendMessage \\(chrome://system-app-test/',
    'at async MessagePipeBrowserTest.',
    // Error stack of the untrusted context.
    'Error from chrome-untrusted://system-app-test',
    'Error: No handler registered for message type \'unknown-message\'',
    'at MessagePipe.receiveMessage_ \\(chrome-untrusted://system-app-test/',
    'at messageListener_ \\(chrome-untrusted://system-app-test/',
  ]);
  testDone();
});

// Tests that we receive an error if the handler fails.
TEST_F('MessagePipeBrowserTest', 'ReceivesProxiedError', async () => {
  const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
  window['untrustedMessagePipe'].logClientError = error =>
      console.log(JSON.stringify(error));
  let caughtError = {};

  try {
    await sendTestMessage('bad-handler');
  } catch (error) {
    caughtError = error;
  }

  assertEquals(caughtError.name, 'Error');
  assertEquals(
      caughtError.message, 'bad-handler: This is an error from untrusted');
  assertMatchErrorStack(caughtError.stack, [
    // Error stack of the test context.
    'Error: bad-handler: This is an error from untrusted',
    'at MessagePipe.sendMessage \\(chrome://system-app-test/',
    'at async MessagePipeBrowserTest.',
    // Error stack of the untrusted context.
    'Error from chrome-untrusted://system-app-test',
    'Error: This is an error from untrusted',
    'at chrome-untrusted://system-app-test/test_data/message_pipe_browsertest_untrusted.js',
    'at MessagePipe.callHandlerForMessageType_ \\(chrome-untrusted://system-app-test/',
    'at MessagePipe.receiveMessage_ \\(chrome-untrusted://system-app-test/',
    'at messageListener_ \\(chrome-untrusted://system-app-test/',
  ]);
  testDone();
});

// Tests `MessagePipe.sendMessage()` properly propagates errors and appends
// stacktraces.
TEST_F('MessagePipeBrowserTest', 'CrossContextErrors', async () => {
  const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
  const untrustedMessagePipe = window['untrustedMessagePipe'];

  untrustedMessagePipe.logClientError = error =>
      console.log(JSON.stringify(error));
  untrustedMessagePipe.rethrowErrors = false;

  untrustedMessagePipe.registerHandler('bad-handler', () => {
    throw Error('This is an error from trusted');
  });

  let caughtError = {};

  try {
    await sendTestMessage('request-bad-handler');
  } catch (e) {
    caughtError = e;
  }

  assertEquals(caughtError.name, 'Error');
  assertEquals(
      caughtError.message,
      'request-bad-handler: bad-handler: This is an error from trusted');
  assertMatchErrorStack(caughtError.stack, [
    // Error stack of the test context.
    'Error: request-bad-handler: bad-handler: This is an error from trusted',
    'at MessagePipe.sendMessage \\(chrome://system-app-test/',
    'at async MessagePipeBrowserTest',
    // Error stack of the untrusted context.
    'Error from chrome-untrusted://system-app-test',
    'Error: bad-handler: This is an error from trusted',
    'at MessagePipe.sendMessage \\(chrome-untrusted://system-app-test/',
    'at async MessagePipe.callHandlerForMessageType_ \\(chrome-untrusted://system-app-test/',
    // Error stack of the trusted context.
    'Error from chrome://system-app-test',
    'Error: This is an error from trusted',
    'at .*message_pipe_browsertest.js',
    'at MessagePipe.callHandlerForMessageType_',
    'at MessagePipe.receiveMessage_',
    'at messageListener_',
  ]);
  testDone();
});
