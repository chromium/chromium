// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://contextual-tasks/composebox.js';

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {getArrayBufferFromBigBuffer, HANDSHAKE_INTERVAL_MS, isGoogleOrigin, MAX_HANDSHAKE_ATTEMPTS} from 'chrome://contextual-tasks/utils.js';

declare global {
  interface Window {
    testState?: {
      targetOrigin: string|null,
      handshakeCompleted: boolean,
    };
  }
}

const browserProxy = ExtensionBrowserProxyImpl.getInstance();

let targetOrigin: string|null = null;
let handshakeIntervalId: number|null = null;
let handshakeCompleted = false;

// For testing
window.testState = {
  get targetOrigin() {
    return targetOrigin;
  },
  get handshakeCompleted() {
    return handshakeCompleted;
  },
};


async function initHandshake() {
  const {message} = await browserProxy.handler.getHandshakeMessage();
  const buffer = getArrayBufferFromBigBuffer(message.smuggled);
  const messageArray = new Uint8Array(buffer);

  // Try to get target origin from referrer
  const referrer = document.referrer;
  if (referrer) {
    try {
      const url = new URL(referrer);
      if (isGoogleOrigin(url.origin)) {
        targetOrigin = url.origin;
      }
    } catch (e) {
      // Ignore
    }
  }

  let handshakeAttempts = 0;

  // Start sending handshake ping on interval
  handshakeIntervalId = setInterval(() => {
    if (handshakeCompleted) {
      if (handshakeIntervalId) {
        clearInterval(handshakeIntervalId);
      }
      return;
    }
    if (handshakeAttempts >= MAX_HANDSHAKE_ATTEMPTS) {
      console.warn('Max handshake attempts reached. Stopping.');
      if (handshakeIntervalId) {
        clearInterval(handshakeIntervalId);
      }
      return;
    }
    handshakeAttempts++;
    const destOrigin = targetOrigin || '*';
    window.parent.postMessage(messageArray, destOrigin);
  }, HANDSHAKE_INTERVAL_MS);
}

// Listen for messages from GWS parent frame
window.addEventListener('message', (event: MessageEvent) => {
  if (event.source !== window.parent) {
    return;
  }

  if (!isGoogleOrigin(event.origin)) {
    console.warn('Rejected message from untrusted origin:', event.origin);
    return;
  }

  if (!targetOrigin) {
    targetOrigin = event.origin;
  } else if (targetOrigin !== event.origin) {
    console.warn(
        'Origin mismatch. Expected:', targetOrigin, 'Got:', event.origin);
    return;
  }

  if (typeof event.data === 'string') {
    // Ignore string messages like 'domContentLoaded' for now.
    return;
  }

  let dataBytes: Uint8Array;
  if (event.data instanceof ArrayBuffer) {
    dataBytes = new Uint8Array(event.data);
  } else if (event.data instanceof Uint8Array) {
    dataBytes = event.data;
  } else {
    console.warn('Unexpected message data type:', typeof event.data);
    return;
  }

  browserProxy.handler.onWebviewMessage(Array.from(dataBytes));
});

// Mojo callbacks
browserProxy.callbackRouter.onHandshakeComplete.addListener(() => {
  handshakeCompleted = true;
  if (handshakeIntervalId) {
    clearInterval(handshakeIntervalId);
  }
});

browserProxy.callbackRouter.postAimMessage.addListener((message: number[]) => {
  if (!targetOrigin) {
    console.error('Cannot post message, target origin not established');
    return;
  }
  const messageArray = new Uint8Array(message);
  window.parent.postMessage(messageArray, targetOrigin);
});

// Start handshake
initHandshake();
