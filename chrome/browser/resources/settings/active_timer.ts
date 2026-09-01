// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tracks the amount of time a user actively engages with a page.
 *
 * This timer uses the Page Lifecycle API to accurately measure active time and
 * emits a metric when the page is closed, navigated away from, or enters the
 * back/forward cache (BFCache). It pauses the timer when the page loses focus
 * or becomes hidden, reducing inaccuracies for long background sessions.
 */
export class ActiveTimer {
  private activeStartTime_: number = 0;
  private totalActiveTime_: number = 0;
  private isRunning_: boolean = false;
  private onEmit_: (duration: number) => void;
  private metricEmitted_: boolean = false;
  private listenersAdded_: boolean = false;

  // Triggered when switching tabs or minimizing the window.
  private visibilityChangeListener_ = () => this.updateTimer_();
  // Triggered when clicking into the page.
  private focusListener_ = () => this.updateTimer_();
  // Triggered when clicking out of the page (e.g., focusing the URL bar).
  private blurListener_ = () => this.updateTimer_();

  // `pagehide` fires when navigating away, closing the tab, or entering
  // BFCache. This serves as our reliable signal that the user is done with the
  // page. We avoid `unload` and `beforeunload` as they are unreliable and
  // prevent the page from being eligible for BFCache.
  private pagehideListener_ = () => this.emit_();

  // `pageshow` fires on initial load, and also when restored from BFCache.
  private pageshowListener_ = (e: PageTransitionEvent) => {
    // `e.persisted` is true if the page was restored from the BFCache.
    // In this case, we treat it as a new session and reset the timer state.
    if (e.persisted) {
      this.resetState_();
      this.updateTimer_();
    }
  };

  constructor(onEmit: (duration: number) => void) {
    this.onEmit_ = onEmit;
  }

  private resetState_() {
    this.activeStartTime_ = 0;
    this.totalActiveTime_ = 0;
    this.isRunning_ = false;
    this.metricEmitted_ = false;
  }

  start() {
    this.resetState_();
    this.updateTimer_();

    if (!this.listenersAdded_) {
      document.addEventListener(
          'visibilitychange', this.visibilityChangeListener_);
      window.addEventListener('focus', this.focusListener_);
      window.addEventListener('blur', this.blurListener_);
      window.addEventListener('pagehide', this.pagehideListener_);
      window.addEventListener('pageshow', this.pageshowListener_);
      this.listenersAdded_ = true;
    }
  }

  stop() {
    if (this.listenersAdded_) {
      document.removeEventListener(
          'visibilitychange', this.visibilityChangeListener_);
      window.removeEventListener('focus', this.focusListener_);
      window.removeEventListener('blur', this.blurListener_);
      window.removeEventListener('pagehide', this.pagehideListener_);
      window.removeEventListener('pageshow', this.pageshowListener_);
      this.listenersAdded_ = false;
    }
    this.emit_();
  }

  private isActive_(): boolean {
    return document.visibilityState === 'visible' && document.hasFocus();
  }

  private startTimer_() {
    if (this.isRunning_) {
      return;
    }
    this.activeStartTime_ = performance.now();
    this.isRunning_ = true;
  }

  private pauseTimer_() {
    if (!this.isRunning_) {
      return;
    }
    this.totalActiveTime_ += performance.now() - this.activeStartTime_;
    this.isRunning_ = false;
  }

  private updateTimer_() {
    if (this.isActive_()) {
      this.startTimer_();
    } else {
      this.pauseTimer_();
    }
  }

  private emit_() {
    if (this.metricEmitted_) {
      return;
    }
    this.pauseTimer_();
    if (this.totalActiveTime_ > 0) {
      this.onEmit_(Math.floor(this.totalActiveTime_));
    }
    this.metricEmitted_ = true;
  }
}
