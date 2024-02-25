// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const LONG_TOUCH_TIME_MS = 1000;

export class LongTouchDetector {
  private callback: () => void;
  /**
   * This is timeout ID used to kill window timeout that fires "detected"
   * callback if touch event was cancelled.
   */
  private timeoutId: null|number;

  constructor(element: HTMLElement, callback: () => void) {
    this.callback = callback;
    this.timeoutId = null;

    element.addEventListener('touchstart', () => void this.onTouchStart());
    element.addEventListener('touchend', () => void this.killTimer());
    element.addEventListener('touchcancel', () => void this.killTimer());

    element.addEventListener('mousedown', () => void this.onTouchStart());
    element.addEventListener('mouseup', () => void this.killTimer());
    element.addEventListener('mouseleave', () => void this.killTimer());
  }

  private onTimeout(): void {
    this.killTimer();
    this.callback();
  }

  private onTouchStart(): void {
    this.killTimer();
    this.timeoutId =
        window.setTimeout(() => void this.onTimeout(), LONG_TOUCH_TIME_MS);
  }

  private killTimer(): void {
    if (this.timeoutId === null) {
      return;
    }

    window.clearTimeout(this.timeoutId);
    this.timeoutId = null;
  }
}
