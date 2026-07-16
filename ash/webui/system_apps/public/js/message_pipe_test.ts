// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import type {MessagePipe} from './message_pipe.js';

declare global {
  interface Window {
    testMessageHandlersReady: Promise<void>;
    untrustedMessagePipe: MessagePipe;
  }
}

/**
 * Wraps regex matching allowing tests to use `assertMatch`.
 */
function assertMatch(str: string, regex: string, opt_message?: string) {
  assertTrue(new RegExp(regex).test(str), opt_message);
}

/**
 * Use to match error stack traces.
 */
function assertMatchErrorStack(
    stackTrace: string, regexLines: string[], opt_message?: string) {
  const regex = `[\\s\\S]*${regexLines.join('[\\s\\S]*')}[\\s\\S]*`;
  assertMatch(stackTrace, regex, opt_message);
}

async function sendTestMessage(
    messageType: string, message: Record<string, any> = {}): Promise<any> {
  await window.testMessageHandlersReady;
  return window.untrustedMessagePipe.sendMessage(messageType, message);
}

suite('MessagePipeTest', () => {
  test('ReceivesSuccessResponse', async () => {
    const request = {'foo': 'bar'};
    const response = await sendTestMessage('success-message', request);
    assertDeepEquals(response, {'success': true, 'request': request});
  });

  test('IgnoresMessagesWithNoType', async () => {
    await sendTestMessage('install-generic-responder');

    await new Promise<void>(resolve => {
      let messageCount = 0;
      const receiveMessage = (event: MessageEvent) => {
        messageCount++;
        // There should be one 'response' for each of the postMessages below.
        // There should be no response from parentMessagePipe because it should
        // ignore the messages below.
        assertEquals(event.data, 'test-response');
        if (messageCount === 5) {
          window.removeEventListener('message', receiveMessage, false);
          resolve();
        }
      };
      window.addEventListener('message', receiveMessage, false);
      const TEST_GUEST_ORIGIN = 'chrome-untrusted://system-app-test';
      const guestFrame = document.querySelector<HTMLIFrameElement>(
          `iframe[src^='${TEST_GUEST_ORIGIN}']`);
      assertTrue(!!guestFrame && !!guestFrame.contentWindow);
      // These postMessages should be ignored and not cause any errors.
      guestFrame.contentWindow.postMessage('test', TEST_GUEST_ORIGIN);
      guestFrame.contentWindow.postMessage({type: 9}, TEST_GUEST_ORIGIN);
      guestFrame.contentWindow.postMessage({}, TEST_GUEST_ORIGIN);
      guestFrame.contentWindow.postMessage(null, TEST_GUEST_ORIGIN);
      guestFrame.contentWindow.postMessage(undefined, TEST_GUEST_ORIGIN);
    });
  });

  test('ReceivesNoHandlerError', async () => {
    window.untrustedMessagePipe.logClientError = error =>
        console.log(JSON.stringify(error, Object.getOwnPropertyNames(error)));
    let caughtError: Record<string, any> = {};

    try {
      await sendTestMessage('unknown-message');
    } catch (error) {
      caughtError = error as Record<string, any>;
    }

    assertEquals(caughtError['name'], 'Error');
    assertEquals(
        caughtError['message'],
        'unknown-message: No handler registered for message type ' +
            '\'unknown-message\'');
    assertMatchErrorStack(caughtError['stack'], [
      // Error stack of the test context.
      'Error: unknown-message: No handler registered for message ' +
          'type \'unknown-message\'',
      'at MessagePipe.sendMessage \\(chrome://system-app-test/',
      'at .*Context.<anonymous>',
      // Error stack of the untrusted context.
      'Error from chrome-untrusted://system-app-test',
      'Error: No handler registered for message type \'unknown-message\'',
      'at MessagePipe.receiveMessage_ \\(chrome-untrusted://system-app-test/',
      'at messageListener_ \\(chrome-untrusted://system-app-test/',
    ]);
  });

  test('ReceivesProxiedError', async () => {
    window.untrustedMessagePipe.logClientError = error =>
        console.log(JSON.stringify(error, Object.getOwnPropertyNames(error)));
    let caughtError: Record<string, any> = {};

    try {
      await sendTestMessage('bad-handler');
    } catch (error) {
      caughtError = error as Record<string, any>;
    }

    assertEquals(caughtError['name'], 'Error');
    assertEquals(
        caughtError['message'], 'bad-handler: This is an error from untrusted');
    assertMatchErrorStack(caughtError['stack'], [
      // Error stack of the test context.
      'Error: bad-handler: This is an error from untrusted',
      'at MessagePipe.sendMessage \\(chrome://system-app-test/',
      'at .*Context.<anonymous>',
      // Error stack of the untrusted context.
      'Error from chrome-untrusted://system-app-test',
      'Error: This is an error from untrusted',
      'at chrome-untrusted://system-app-test/test_data/' +
          'message_pipe_browsertest_untrusted.js',
      'at MessagePipe.callHandlerForMessageType_ \\(' +
          'chrome-untrusted://system-app-test/',
      'at MessagePipe.receiveMessage_ \\(chrome-untrusted://system-app-test/',
      'at messageListener_ \\(chrome-untrusted://system-app-test/',
    ]);
  });

  test('CrossContextErrors', async () => {
    const untrustedMessagePipe = window.untrustedMessagePipe;

    untrustedMessagePipe.logClientError = error =>
        console.log(JSON.stringify(error, Object.getOwnPropertyNames(error)));
    untrustedMessagePipe.rethrowErrors = false;

    untrustedMessagePipe.registerHandler('bad-handler', () => {
      throw Error('This is an error from trusted');
    });

    let caughtError: Record<string, any> = {};

    try {
      await sendTestMessage('request-bad-handler');
    } catch (e) {
      caughtError = e as Record<string, any>;
    }

    assertEquals(caughtError['name'], 'Error');
    assertEquals(
        caughtError['message'],
        'request-bad-handler: bad-handler: This is an error from trusted');
    assertMatchErrorStack(caughtError['stack'], [
      // Error stack of the test context.
      'Error: request-bad-handler: bad-handler: This is an error from trusted',
      'at MessagePipe.sendMessage \\(chrome://system-app-test/',
      'at .*Context.<anonymous>',
      // Error stack of the untrusted context.
      'Error from chrome-untrusted://system-app-test',
      'Error: bad-handler: This is an error from trusted',
      'at MessagePipe.sendMessage \\(chrome-untrusted://system-app-test/',
      'at async MessagePipe.callHandlerForMessageType_ \\(' +
          'chrome-untrusted://system-app-test/',
      // Error stack of the trusted context.
      'Error from chrome://system-app-test',
      'Error: This is an error from trusted',
      'at .*message_pipe_test.js',
      'at MessagePipe.callHandlerForMessageType_',
      'at MessagePipe.receiveMessage_',
      'at messageListener_',
    ]);
  });
});
