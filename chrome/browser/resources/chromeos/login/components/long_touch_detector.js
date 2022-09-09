// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const LONG_TOUCH_TIME_MS = 1000;

/* #export */ class LongTouchDetector {
  constructor(element, callback) {
    this.callback_ = callback;
    /**
     * This is timeout ID used to kill window timeout that fires "detected"
     * callback if touch event was cancelled.
     *
     * @private {number|null}
     */
    this.timeoutId_ = null;

    element.addEventListener('touchstart', () => void this.onTouchStart_());
    element.addEventListener('touchend', () => void this.killTimer_());
    element.addEventListener('touchcancel', () => void this.killTimer_());

    element.addEventListener('mousedown', () => void this.onTouchStart_());
    element.addEventListener('mouseup', () => void this.killTimer_());
    element.addEventListener('mouseleave', () => void this.killTimer_());
  }

  /**
   *  window.setTimeout() callback.
   *
   * @private
   */
  onTimeout_() {
    this.killTimer_();
    this.callback_();
  }

  /**
   * @private
   */
  onTouchStart_() {
    this.killTimer_();
    this.timeoutId_ =
        window.setTimeout(() => void this.onTimeout_(), LONG_TOUCH_TIME_MS);
  }

  /**
   * @private
   */
  killTimer_() {
    if (this.timeoutId_ === null) {
      return;
    }

    window.clearTimeout(this.timeoutId_);
    this.timeoutId_ = null;
  }
}
