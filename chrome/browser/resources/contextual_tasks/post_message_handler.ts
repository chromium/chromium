// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';

const HANDSHAKE_INTERVAL_MS = 500;
const MAX_HANDSHAKE_ATTEMPTS = 1000;

export interface Rect {
  top: number;
  left: number;
  width: number;
  right: number;
  bottom: number;
  height: number;
}

export interface InputPlateBoundsUpdateMessage {
  type: 'input-plate-bounds-update';
  'bounds-rect': Rect;
  occluders: Rect[];
}

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
  private pendingMessages_: Array<Uint8Array|object> = [];
  private handshakeMessage_: Uint8Array|null = null;
  private onInputPlateBoundsUpdate_:
      ((rect?: Rect, occluders?: Rect[]) => void)|null = null;

  constructor(
      webview: chrome.webviewTag.WebView, browserProxy: BrowserProxy,
      // Allow overriding max attempts for testing.
      private readonly maxHandshakeAttempts_: number = MAX_HANDSHAKE_ATTEMPTS) {
    this.webview_ = webview;
    this.browserProxy_ = browserProxy;

    this.eventTracker_.add(
        this.webview_, 'loadstart', this.onLoadStart_.bind(this));
    this.eventTracker_.add(
        this.webview_, 'loadcommit', this.onLoadCommit_.bind(this));
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

  /**
   * DO NOT USE! This is temporary to prove a proof of concept. Eventually,
   * this should be changed to send a serialized proto like the other method.
   *
   * Sends an object message to the webview. If the handshake has not yet been
   * acknowledged, the message will be queued and sent after the handshake is
   * complete.
   * @param message The object message to send.
   *
   * TODO(crbug.com/483737358): Remove this method once the proto is implemented
   * on the webview side.
   */
  sendObjectMessage(message: object) {
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

  private onLoadStart_(event: chrome.webviewTag.LoadStartEvent) {
    // This event is fired anytime a load starts in the webview, including
    // subframes and navigations within the same page. Only reset the handshake
    // if its the top level frame to avoid unnecessary resets.
    if (!event.isTopLevel) {
      return;
    }

    // Reset the handshake since the src has changed.
    this.resetHandshake_();
  }

  // This event is fired when the load has committed in the webview. This is
  // used to determine if the webview is ready to receive messages. Doing this
  // in onLoadStart_ can cause a race condition where the handshake completes
  // with the page that is about to navigate away from.
  private onLoadCommit_(event: chrome.webviewTag.LoadCommitEvent) {
    if (!event.isTopLevel) {
      return;
    }

    this.targetOrigin_ = new URL(event.url).origin;

    // Must reset the handshake before starting a new one. onLoadCommit_ can be
    // called multiple times in a row for the same page load, so reattempt the
    // handshake with each page load, since there is no way to distinguish
    // which load has the receiving Javascript.
    this.resetHandshake_();
    this.startHandshake_();
  }

  private resetHandshake_() {
    this.handshakeComplete_ = false;
    this.handshakeAttempts_ = 0;
    this.stopHandshake_();
  }

  private startHandshake_() {
    if (this.handshakeIntervalId_ !== null || this.handshakeComplete_) {
      // If the handshake is already in progress, do not start a new one.
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

    // TODO(crbug.com/483737358): Sending an object instead of a proto is
    // a temporary solution to unblock the prototype. Remove this method
    // once the proto is implemented on the webview side.
    if (event.data && event.data.type === 'input-plate-bounds-update') {
      if (this.onInputPlateBoundsUpdate_) {
        this.onInputPlateBoundsUpdate_(
            event.data['bounds-rect'], event.data['occluders']);
      }
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

  setInputPlateBoundsUpdateCallback(
      callback: (rect?: Rect, occluders?: Rect[]) => void) {
    this.onInputPlateBoundsUpdate_ = callback;
  }

  completeHandshake() {
    if (this.handshakeComplete_) {
      return;
    }
    this.handshakeComplete_ = true;
    this.stopHandshake_();

    this.pendingMessages_.forEach(msg => this.postMessage_(msg), this);
    this.pendingMessages_ = [];
  }

  getPendingMessagesLengthForTesting(): number {
    return this.pendingMessages_.length;
  }

  isHandshakeCompleteForTesting(): boolean {
    return this.handshakeComplete_;
  }

  private postMessage_(message: Uint8Array|object) {
    if (!this.webview_.contentWindow) {
      return;
    }
    if (this.targetOrigin_ === 'null' || !this.targetOrigin_) {
      return;
    }
    try {
      if (message instanceof Uint8Array) {
        this.webview_.contentWindow.postMessage(
            message.buffer, this.targetOrigin_);
      } else {
        this.webview_.contentWindow.postMessage(message, this.targetOrigin_);
      }
    } catch (e) {
      console.error('Failed to postMessage to webview:', e);
    }
  }
}
