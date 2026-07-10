// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import {ToastType} from '../skills.mojom-webui.js';

import {HANDSHAKE_PING_INTERVAL_MS, HANDSHAKE_TIMEOUT_MS, PRIMARY_SKILLS_ORIGIN, SKILLS_API_ALLOWED_ORIGINS, SKILLS_HANDSHAKE_ACK, SKILLS_HANDSHAKE_TYPE, SKILLS_SHOW_TOAST} from './skills_webview_bridge_constants.js';

/**
 * Returns a URLPattern given an origin pattern string that has the syntax:
 * <protocol>://<hostname>[:<port>]
 * where <protocol>, <hostname> and <port> are inserted into URLPattern.
 */
export function matcherForOrigin(originPattern: string): URLPattern|null {
  const match = originPattern.match(/([^:]+):\/\/([^:]*)(?::(\d+))?[/]?/);
  if (!match) {
    return null;
  }

  const [protocol, hostname, port] = [match[1], match[2], match[3] ?? '*'];
  try {
    return new URLPattern({protocol, hostname, port});
  } catch (_) {
    return null;
  }
}

function isInternalOnlyOrigin(origin: string): boolean {
  return origin === 'https://login.corp.google.com' ||
      origin === 'https://accounts.googlers.com';
}

export function urlMatchesApiAllowedOrigin(url: URL): boolean {
  if (url.origin === 'null') {
    return false;
  }

  // For development and testing.
  if (loadTimeData.getBoolean('devMode')) {
    return true;
  }

  // A URL is allowed to have API access if it matches any of the explicit API
  // allowed origins.
  return SKILLS_API_ALLOWED_ORIGINS.some((origin: string) => {
    // Only allow internal origins for internal users.
    if (isInternalOnlyOrigin(origin) &&
        !loadTimeData.getBoolean('isInternalUser')) {
      return false;
    }
    return matcherForOrigin(origin.trim())?.test(url);
  });
}

// TODO(b/529400161): Consider moving to another file.
export interface SkillsWebviewBridgeDelegate {
  onError(): void;
  onShowToast(toastType: ToastType): void;
}

/**
 * A bridge class that manages the postMessage handshake and communication
 * between the Chrome WebUI host and the guest Webview application.
 */
export class SkillsWebviewBridge {
  private webview_: chrome.webviewTag.WebView;
  private targetOrigin_: string = '';
  private handshakeIntervalId_: number|null = null;
  private timeoutId_: number|null = null;
  private isConnected_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();
  private delegate_: SkillsWebviewBridgeDelegate;

  constructor(
      webview: chrome.webviewTag.WebView,
      delegate: SkillsWebviewBridgeDelegate) {
    assert(loadTimeData.getBoolean('isSkillsWebViewV2Enabled'));
    this.webview_ = webview;
    this.delegate_ = delegate;

    this.eventTracker_.add(
        this.webview_, 'loadcommit',
        (e: Event) =>
            this.onLoadCommit(e as chrome.webviewTag.LoadCommitEvent));
    this.eventTracker_.add(this.webview_, 'loadstop', () => this.onLoadStop());
    this.eventTracker_.add(
        window, 'message', (e: MessageEvent) => this.onMessage(e));
  }

  private onLoadCommit(e: chrome.webviewTag.LoadCommitEvent) {
    if (!e.isTopLevel) {
      return;
    }

    const urlObj = URL.parse(e.url);

    // Disallowed Origin.
    if (!urlObj || !urlMatchesApiAllowedOrigin(urlObj)) {
      this.delegate_.onError();
      return;
    }

    // Start handshake if valid target url.
    if (this.urlRequiresHandshake(urlObj)) {
      this.targetOrigin_ = urlObj.origin;
      this.webview_.setAttribute('hidden', 'true');
      this.startHandshake();
    }
  }

  private urlRequiresHandshake(url: URL): boolean {
    // If we are already connected we don't need a new handshake.
    if (this.isConnected_) {
      return false;
    }
    // For development and testing.
    if (loadTimeData.getBoolean('devMode')) {
      return true;
    }

    return matcherForOrigin(PRIMARY_SKILLS_ORIGIN)?.test(url) ?? false;
  }

  private onLoadStop() {
    if (this.webview_.checkVisibility?.()) {
      this.webview_.focus();
    }
  }

  private startHandshake() {
    // Reset in case of successive handshakes.
    this.isConnected_ = false;
    this.stopHandshake();

    // Send a handshake ping periodically.
    this.handshakeIntervalId_ = window.setInterval(() => {
      this.sendPing();
    }, HANDSHAKE_PING_INTERVAL_MS);

    // Set a timeout to abort handshake.
    this.timeoutId_ = window.setTimeout(() => {
      this.stopHandshake();
      this.delegate_.onError();
    }, HANDSHAKE_TIMEOUT_MS);

    this.sendPing();
  }

  private sendPing() {
    if (!this.targetOrigin_) {
      return;
    }
    if (this.webview_.contentWindow) {
      this.webview_.contentWindow.postMessage(
          {type: SKILLS_HANDSHAKE_TYPE}, this.targetOrigin_);
    }
  }

  private stopHandshake() {
    if (this.handshakeIntervalId_ !== null) {
      window.clearInterval(this.handshakeIntervalId_);
      this.handshakeIntervalId_ = null;
    }
    if (this.timeoutId_ !== null) {
      window.clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    }
  }

  private onMessage(e: MessageEvent) {
    if (this.webview_.contentWindow &&
        e.source !== this.webview_.contentWindow) {
      return;
    }
    if (this.targetOrigin_ && e.origin !== this.targetOrigin_) {
      return;
    }
    if (!e.data) {
      return;
    }

    // Handle handshake ack if guest replies.
    if (e.data.type === SKILLS_HANDSHAKE_ACK) {
      this.isConnected_ = true;
      this.webview_.removeAttribute('hidden');
      this.stopHandshake();
    }

    // Before we process non-handshake message, make sure we are connected.
    if (!this.isConnected_) {
      return;
    }

    if (e.data.type === SKILLS_SHOW_TOAST) {
      this.handleShowToastMessage(e.data);
    }
  }

  private handleShowToastMessage(data: {toastType: string}) {
    // TODO(b/529405584): Refactor toastType to be an enum & consider how we
    // want to surface errors to the user if skillId does not exist.
    if (data.toastType === 'save') {
      this.delegate_.onShowToast(ToastType.kSave);
    } else if (data.toastType === 'delete') {
      this.delegate_.onShowToast(ToastType.kDelete);
    }
  }

  destroy() {
    this.stopHandshake();
    this.eventTracker_.removeAll();
  }

  isConnected(): boolean {
    return this.isConnected_;
  }
}
