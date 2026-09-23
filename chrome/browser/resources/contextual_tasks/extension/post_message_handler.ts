// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ExtensionPageInterface} from 'chrome://contextual-tasks/contextual_tasks.mojom-webui.js';
import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ExtensionBrowserProxy} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {getArrayBufferFromBigBuffer, HANDSHAKE_INTERVAL_MS, MAX_HANDSHAKE_ATTEMPTS} from 'chrome://contextual-tasks/utils.js';

type ProtoWrapper = Parameters<ExtensionPageInterface['postSearchMessage']>[0];

const MAX_MESSAGE_BYTES = 1024 * 1024;  // 1MB

// LINT.IfChange(AllowedOrigins)
export function urlMatchesAllowList(origin: string): boolean {
  try {
    const url = new URL(origin);
    if (url.protocol !== 'https:') {
      return false;
    }
    const host = url.hostname;
    return host === 'google.com' || host === 'www.google.com' ||
        host.endsWith('.borg.google.com') ||
        host.endsWith('.corp.google.com') || host.endsWith('.prod.google.com');
  } catch {
    return false;
  }
}
// LINT.ThenChange(
//   manifest.json:AllowedOrigins,
//   manifest.json:WebAccessibleMatches
// )

export class ExtensionPostMessageHandler {
  private browserProxy_: ExtensionBrowserProxy;
  private targetOrigin_: string|null = null;
  private pendingSearchMessages_: Uint8Array[] = [];
  private handshakeIntervalId_: number|null = null;
  private handshakeCompleted_: boolean = false;
  private messageListener_: ((event: MessageEvent) => void)|null = null;
  private onHandshakeCompleteListenerId_: number|null = null;
  private postAimMessageListenerId_: number|null = null;
  private postSearchMessageListenerId_: number|null = null;
  private isDestroyed_: boolean = false;

  constructor(browserProxy?: ExtensionBrowserProxy) {
    this.browserProxy_ =
        browserProxy || ExtensionBrowserProxyImpl.getInstance();
    this.initMojoListeners_();
    this.initWindowListener_();
    this.initHandshake();
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

  private setTargetOrigin_(origin: string) {
    if (!urlMatchesAllowList(origin)) {
      return;
    }
    this.targetOrigin_ = origin;
    while (this.pendingSearchMessages_.length > 0) {
      const msg = this.pendingSearchMessages_.shift()!;
      window.parent.postMessage(msg, this.targetOrigin_);
    }
  }

  setTargetOriginForTesting(origin: string|null) {
    if (origin) {
      this.setTargetOrigin_(origin);
    } else {
      this.targetOrigin_ = null;
    }
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
        this.setTargetOrigin_(url.origin);
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
      if (!this.targetOrigin_) {
        // Do not broadcast internal protocol messages if target origin cannot
        // be authoritatively determined from referrer.
        return;
      }
      handshakeAttempts++;
      window.parent.postMessage(messageArray, this.targetOrigin_);
    }, HANDSHAKE_INTERVAL_MS);
  }

  private initWindowListener_() {
    this.messageListener_ = (event: MessageEvent) => {
      if (event.source !== window.parent) {
        return;
      }

      if (!urlMatchesAllowList(event.origin)) {
        console.warn('Rejected message from untrusted origin:', event.origin);
        return;
      }

      if (!this.targetOrigin_) {
        this.setTargetOrigin_(event.origin);
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

      if (dataBytes.byteLength > MAX_MESSAGE_BYTES) {
        console.warn(
            'Message exceeds maximum allowed size:', dataBytes.byteLength);
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

    this.postSearchMessageListenerId_ =
        this.browserProxy_.callbackRouter.postSearchMessage.addListener(
            (message: ProtoWrapper) => {
              const buffer = getArrayBufferFromBigBuffer(message.smuggled);
              const messageArray = new Uint8Array(buffer);
              if (this.targetOrigin_) {
                window.parent.postMessage(messageArray, this.targetOrigin_);
              } else {
                this.pendingSearchMessages_.push(messageArray);
              }
            });
  }

  destroy() {
    this.isDestroyed_ = true;
    this.pendingSearchMessages_ = [];
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
    if (this.postSearchMessageListenerId_ !== null) {
      this.browserProxy_.callbackRouter.removeListener(
          this.postSearchMessageListenerId_);
      this.postSearchMessageListenerId_ = null;
    }
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
