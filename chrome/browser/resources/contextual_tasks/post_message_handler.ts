// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import type {WebViewType} from './web_view_type.js';

const HANDSHAKE_INTERVAL_MS = 10;
// 3000 * 10ms = 30 seconds.
const MAX_HANDSHAKE_ATTEMPTS = 3000;

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
  private webview_: WebViewType;
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
  private lastLoadStartEvent_: chrome.webviewTag.LoadStartEvent|null = null;

  constructor(
      webview: WebViewType, browserProxy: BrowserProxy,
      // Allow overriding max attempts for testing.
      private readonly maxHandshakeAttempts_: number = MAX_HANDSHAKE_ATTEMPTS) {
    this.webview_ = webview;
    this.browserProxy_ = browserProxy;

    this.eventTracker_.add(
        this.webview_, 'loadstart', this.onLoadStart_.bind(this));
    this.eventTracker_.add(
        this.webview_, 'loadredirect', this.onLoadRedirect_.bind(this));
    this.eventTracker_.add(
        this.webview_, 'loadcommit', this.onLoadCommit_.bind(this));
    this.eventTracker_.add(
        this.webview_, 'loadabort', this.onLoadAbort_.bind(this));

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
    if (!event.isTopLevel) {
      return;
    }
    // Store the start event to be used once its clear if this navigation is
    // committed or aborted
    this.lastLoadStartEvent_ = event;
  }

  private onLoadRedirect_(event: chrome.webviewTag.LoadRedirectEvent) {
    if (!event.isTopLevel) {
      return;
    }

    const urlObj = URL.parse(event.newUrl);
    if (urlObj) {
      // Update target origin immediately on redirect to ensure handshake
      // messages go to the correct origin for the intermediate hop or final
      // destination.
      this.targetOrigin_ = urlObj.origin;
    } else {
      console.error('Invalid URL in loadredirect:', event.newUrl);
    }

    this.maybeHandleNavigation_(event.oldUrl);
  }

  // This event is fired when the load has committed in the webview. This is
  // used to determine if the webview is ready to receive messages. Doing this
  // in onLoadStart_ can cause a race condition where the handshake completes
  // with the page that is about to navigate away from.
  private onLoadCommit_(event: chrome.webviewTag.LoadCommitEvent) {
    if (!event.isTopLevel) {
      return;
    }

    const urlObj = URL.parse(event.url);
    if (urlObj) {
      // Update target origin to the final committed URL.
      this.targetOrigin_ = urlObj.origin;
    } else {
      console.error('Invalid URL in loadcommit:', event.url);
    }

    this.maybeHandleNavigation_(event.url);
  }

  private onLoadAbort_(event: chrome.webviewTag.LoadAbortEvent) {
    if (!event.isTopLevel) {
      return;
    }
    // The navigation aborted, so reset the last thread frame load start event.
    this.lastLoadStartEvent_ = null;
  }

  // Resets and starts the handshake only if the navigation URL matches the
  // stashed load start event URL. This ensures we only act once per navigation
  // chain (either on the first redirect or on commit if no redirect) and
  // handles aborted navigations correctly.
  private maybeHandleNavigation_(navigationUrl: string) {
    if (this.lastLoadStartEvent_ &&
        this.lastLoadStartEvent_.url === navigationUrl) {
      this.lastLoadStartEvent_ = null;
      this.resetHandshake_();
      this.startHandshake_();
    }
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
