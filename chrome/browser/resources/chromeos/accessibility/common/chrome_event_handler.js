// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class wraps ChromeEvent listeners, adding some convenience
 * functions.
 */
export class ChromeEventHandler {
  /**
   * @param {!ChromeEvent} chromeEvent
   * @param {function(...)} callback
   */
  constructor(chromeEvent, callback) {
    /** @private {!ChromeEvent} */
    this.chromeEvent_ = chromeEvent;

    /** @private {function(...)} */
    this.callback_ = callback;

    /** @private {boolean} */
    this.listening_ = false;
  }

  /** Starts listening to events. */
  start() {
    if (this.listening_) {
      return;
    }

    this.listening_ = true;
    this.chromeEvent_.addListener(this.callback_);
  }

  /** Stops listening or handling future events. */
  stop() {
    this.listening_ = false;
    this.chromeEvent_.removeListener(this.callback_);
  }
}
