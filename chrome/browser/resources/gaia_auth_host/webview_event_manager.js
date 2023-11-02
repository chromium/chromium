// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides WebviewEventManager which can register and keep track of listeners
 * on EventTargets and WebRequests, and unregister all listeners later.
 */

/**
 * Creates a new WebviewEventManager.
 */
export class WebviewEventManager {
  constructor() {
    this.unbindWebviewCleanupFunctions_ = [];
  }

  /**
   * Adds a EventListener to |eventTarget| and adds a clean-up function so we
   * can remove the listener in unbindFromWebview.
   * @param {Object} eventTarget the object to add the listener to
   * @param {string} type the event type
   * @param {Function} listener the event listener
   */
  addEventListener(eventTarget, type, listener) {
    eventTarget.addEventListener(type, listener);
    this.unbindWebviewCleanupFunctions_.push(
        eventTarget.removeEventListener.bind(eventTarget, type, listener));
  }

  /**
   * Adds a listener to |webRequestEvent| and adds a clean-up function so we can
   * remove the listener in unbindFromWebview.
   * @param {Object} webRequestEvent the object to add the listener to.
   * @param {Function} listener the event listener
   * @param {RequestFilter} filter the object describing filters to apply to
   *     webRequest events.
   * @param {?Object} extraInfoSpec the object to pass additional event-specific
   *     instructions.
   */
  addWebRequestEventListener(webRequestEvent, listener, filter, extraInfoSpec) {
    webRequestEvent.addListener(listener, filter, extraInfoSpec);
    this.unbindWebviewCleanupFunctions_.push(
        webRequestEvent.removeListener.bind(webRequestEvent, listener));
  }

  /**
   * Unbinds this Authenticator from the currently bound webview.
   */
  removeAllListeners() {
    for (let i = 0; i < this.unbindWebviewCleanupFunctions_.length; i++) {
      this.unbindWebviewCleanupFunctions_[i]();
    }
    this.unbindWebviewCleanupFunctions_ = [];
  }
}
