// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MessagePipe} from '../message_pipe.js';

/** A pipe through which we can send messages to the untrusted frame. */
const untrustedMessagePipe =
    new MessagePipe('chrome-untrusted://system-app-test');

/**
 * Promise that signals the guest is ready to receive test messages.
 * @type {!Promise<undefined>}
 */
const testMessageHandlersReady = new Promise(resolve => {
  window.addEventListener('DOMContentLoaded', () => {
    untrustedMessagePipe.registerHandler('test-handlers-ready', resolve);
  });
});

// Expose on window so that it can be accessed by message_pipe_browsertest.js.
Object.assign(window, {testMessageHandlersReady, untrustedMessagePipe});
