// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';

const HANDSHAKE_INTERVAL_MS = 500;
const MAX_HANDSHAKE_ATTEMPTS = 1000;

/**
 * A proxy class to control post messages sent to the webview.
 */
export class PostMessageHandler {
  private webview_: chrome.webviewTag.WebView;
  private targetOrigin_: string = '';
  private eventTracker_: EventTracker = new EventTracker();
  private browserProxy_: BrowserProxy;

  private handshakeComplete_: boolean = false;
  private handshakeAttempts_: number = 0;
  private handshakeIntervalId_: number|null = null;
  private pendingMessages_: Uint8Array[] = [];
  private handshakeMessage_: Uint8Array|null = null;

  constructor(
      webview: chrome.webviewTag.WebView, browserProxy: BrowserProxy,
      // Allow overriding max attempts for testing.
      private readonly maxHandshakeAttempts_: number = MAX_HANDSHAKE_ATTEMPTS) {
    this.webview_ = webview;
    this.browserProxy_ = browserProxy;

    this.eventTracker_.add(
        this.webview_, 'loadstop', this.onLoadStop_.bind(this));
    this.eventTracker_.add(
        window, 'message', this.onMessageReceived_.bind(this));

    const encodedHandshakeMessage = loadTimeData.getString('handshakeMessage');
    // TODO(crbug.com/465817042): Switch to Uint8Array.fromBase64 when
    // available.
    this.handshakeMessage_ =
        Uint8Array.from(atob(encodedHandshakeMessage), c => c.charCodeAt(0));
  }

  /**
   * Sends a message to the webview. If the handshake has not yet been
   * acknowledged, the message will be queued and sent after the handshake is
   * complete.
   * @param message The serialized message to send.
   */
  sendMessage(message: Uint8Array) {
    if (!this.handshakeComplete_) {
      this.pendingMessages_.push(message);
      return;
    }

    this.postMessage_(message);
  }


  detach() {
    this.eventTracker_.removeAll();
    this.resetHandshake_();
  }

  private onLoadStop_() {
    this.resetHandshake_();
    if (this.webview_.src) {
      this.targetOrigin_ = new URL(this.webview_.src).origin;
      this.startHandshake_();
    }
  }

  private resetHandshake_() {
    this.handshakeComplete_ = false;
    this.handshakeAttempts_ = 0;
    this.stopHandshake_();
  }

  private startHandshake_() {
    if (this.handshakeIntervalId_ !== null) {
      return;
    }

    assert(this.handshakeMessage_);
    this.handshakeIntervalId_ = setInterval(() => {
      if (this.handshakeComplete_) {
        this.stopHandshake_();
        return;
      }
      if (this.handshakeAttempts_ >= this.maxHandshakeAttempts_) {
        this.stopHandshake_();
        return;
      }
      this.handshakeAttempts_++;

      this.postMessage_(this.handshakeMessage_!);
    }, HANDSHAKE_INTERVAL_MS);
  }

  private stopHandshake_() {
    if (this.handshakeIntervalId_ !== null) {
      clearInterval(this.handshakeIntervalId_);
      this.handshakeIntervalId_ = null;
    }
  }

  private onMessageReceived_(event: MessageEvent) {
    if (event.origin !== this.targetOrigin_) {
      return;
    }

    try {
      // No json messages are expected from the webview.
      JSON.parse(event.data);
    } catch (e) {
      // If JSON parsing fails, assume it's the binary proto message.
      if (event.data instanceof Uint8Array) {
        const messageBytes = Array.from(event.data);
        this.browserProxy_.handler.onWebviewMessage(messageBytes);
      } else if (event.data instanceof ArrayBuffer) {
        const messageBytes = Array.from(new Uint8Array(event.data));
        this.browserProxy_.handler.onWebviewMessage(messageBytes);
      }
    }
  }

  completeHandshake() {
    if (this.handshakeComplete_) {
      return;
    }
    this.handshakeComplete_ = true;
    this.stopHandshake_();

    this.pendingMessages_.forEach(msg => this.sendMessage(msg), this);
    this.pendingMessages_ = [];
  }

  getPendingMessagesLengthForTesting(): number {
    return this.pendingMessages_.length;
  }

  isHandshakeCompleteForTesting(): boolean {
    return this.handshakeComplete_;
  }

  private postMessage_(message: Uint8Array) {
    if (!this.webview_.contentWindow) {
      return;
    }
    if (this.targetOrigin_ === 'null' || !this.targetOrigin_) {
      return;
    }
    try {
      this.webview_.contentWindow.postMessage(
          message.buffer, this.targetOrigin_);
    } catch (e) {
      console.error('Failed to postMessage to webview:', e);
    }
  }
}
