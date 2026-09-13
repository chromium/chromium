// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ExtensionBrowserProxy} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {getArrayBufferFromBigBuffer, HANDSHAKE_INTERVAL_MS, isGoogleOrigin, MAX_HANDSHAKE_ATTEMPTS} from 'chrome://contextual-tasks/utils.js';

declare global {
  interface Window {
    // Exposes read-only handshake tracking state on window for test
    // verification in extension_post_message_handler_test.ts and browser tests.
    // Cleaned up on destroy().
    stateForTesting?: {
      targetOrigin: string|null,
      handshakeCompleted: boolean,
    };
  }
}

export class ExtensionPostMessageHandler {
  private browserProxy_: ExtensionBrowserProxy;
  private targetOrigin_: string|null = null;
  private handshakeIntervalId_: number|null = null;
  private handshakeCompleted_: boolean = false;
  private messageListener_: ((event: MessageEvent) => void)|null = null;
  private onHandshakeCompleteListenerId_: number|null = null;
  private postAimMessageListenerId_: number|null = null;
  private isDestroyed_: boolean = false;

  constructor(browserProxy?: ExtensionBrowserProxy) {
    this.browserProxy_ =
        browserProxy || ExtensionBrowserProxyImpl.getInstance();
    this.initMojoListeners_();
    this.initWindowListener_();
    this.initHandshake();

    const handler = this;
    window.stateForTesting = {
      get targetOrigin() {
        return handler.targetOrigin_;
      },
      get handshakeCompleted() {
        return handler.handshakeCompleted_;
      },
    };
  }

  get targetOrigin(): string|null {
    return this.targetOrigin_;
  }

  get handshakeCompleted(): boolean {
    return this.handshakeCompleted_;
  }

  get isDestroyed(): boolean {
    return this.isDestroyed_;
  }

  async initHandshake(): Promise<void> {
    let messageArray: Uint8Array;
    try {
      const {message} = await this.browserProxy_.handler.getHandshakeMessage();
      if (this.isDestroyed_) {
        return;
      }
      const buffer = getArrayBufferFromBigBuffer(message.smuggled);
      messageArray = new Uint8Array(buffer);
    } catch {
      // Mojo connection disconnected or handshake call failed.
      return;
    }

    // Try to get target origin from referrer
    const referrer = document.referrer;
    if (referrer) {
      try {
        const url = new URL(referrer);
        if (isGoogleOrigin(url.origin)) {
          this.targetOrigin_ = url.origin;
        }
      } catch {
        // Ignore
      }
    }

    let handshakeAttempts = 0;

    if (this.isDestroyed_) {
      return;
    }

    // Start sending handshake ping on interval
    this.handshakeIntervalId_ = window.setInterval(() => {
      if (this.isDestroyed_ || this.handshakeCompleted_) {
        if (this.handshakeIntervalId_ !== null) {
          clearInterval(this.handshakeIntervalId_);
          this.handshakeIntervalId_ = null;
        }
        return;
      }
      if (handshakeAttempts >= MAX_HANDSHAKE_ATTEMPTS) {
        console.warn('Max handshake attempts reached. Stopping.');
        if (this.handshakeIntervalId_ !== null) {
          clearInterval(this.handshakeIntervalId_);
          this.handshakeIntervalId_ = null;
        }
        return;
      }
      handshakeAttempts++;
      const destOrigin = this.targetOrigin_ || '*';
      window.parent.postMessage(messageArray, destOrigin);
    }, HANDSHAKE_INTERVAL_MS);
  }

  private initWindowListener_() {
    this.messageListener_ = (event: MessageEvent) => {
      if (event.source !== window.parent) {
        return;
      }

      if (!isGoogleOrigin(event.origin)) {
        console.warn('Rejected message from untrusted origin:', event.origin);
        return;
      }

      if (!this.targetOrigin_) {
        this.targetOrigin_ = event.origin;
      } else if (this.targetOrigin_ !== event.origin) {
        console.warn(
            'Origin mismatch. Expected:', this.targetOrigin_,
            'Got:', event.origin);
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

      this.browserProxy_.handler.onWebviewMessage(Array.from(dataBytes));
    };

    window.addEventListener('message', this.messageListener_);
  }

  private initMojoListeners_() {
    this.onHandshakeCompleteListenerId_ =
        this.browserProxy_.callbackRouter.onHandshakeComplete.addListener(
            () => {
              this.handshakeCompleted_ = true;
              if (this.handshakeIntervalId_ !== null) {
                clearInterval(this.handshakeIntervalId_);
                this.handshakeIntervalId_ = null;
              }
            });

    this.postAimMessageListenerId_ =
        this.browserProxy_.callbackRouter.postAimMessage.addListener(
            (message: number[]) => {
              if (!this.targetOrigin_) {
                console.error(
                    'Cannot post message, target origin not established');
                return;
              }
              const messageArray = new Uint8Array(message);
              window.parent.postMessage(messageArray, this.targetOrigin_);
            });
  }

  destroy() {
    this.isDestroyed_ = true;
    if (this.handshakeIntervalId_ !== null) {
      clearInterval(this.handshakeIntervalId_);
      this.handshakeIntervalId_ = null;
    }
    if (this.messageListener_) {
      window.removeEventListener('message', this.messageListener_);
      this.messageListener_ = null;
    }
    if (this.onHandshakeCompleteListenerId_ !== null) {
      this.browserProxy_.callbackRouter.removeListener(
          this.onHandshakeCompleteListenerId_);
      this.onHandshakeCompleteListenerId_ = null;
    }
    if (this.postAimMessageListenerId_ !== null) {
      this.browserProxy_.callbackRouter.removeListener(
          this.postAimMessageListenerId_);
      this.postAimMessageListenerId_ = null;
    }
    delete window.stateForTesting;
  }
}

// Holds the singleton instance for the current window/frame context. Each
// extension iframe runs in its own window document and initializes its own
// handler instance.
let instance: ExtensionPostMessageHandler|null = null;

export function initExtensionPostMessaging(
    browserProxy?: ExtensionBrowserProxy): ExtensionPostMessageHandler {
  if (!instance) {
    instance = new ExtensionPostMessageHandler(browserProxy);
  }
  return instance;
}

export function resetExtensionPostMessagingForTesting() {
  if (instance) {
    instance.destroy();
    instance = null;
  }
}

// Auto-initialize when running inside an extension frame.
if (window.location.protocol === 'chrome-extension:') {
  initExtensionPostMessaging();
}
