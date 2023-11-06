// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'OobeScrollableBehavior' is a special behavior which helps to update classes
 * on the scrollable element on size change.
 */

/** @polymerBehavior */
export const OobeScrollableBehavior = {
  /**
   * Init observers to keep track of the scrollable element size changes.
   */
  initScrollableObservers(scrollableElement, ...sizeChangeObservableElemenets) {
    if (!scrollableElement || this.scrollableElement_) {
      return;
    }
    this.scrollableElement_ = scrollableElement;
    this.resizeObserver_ =
        new ResizeObserver(this.applyScrollClassTags_.bind(this));
    this.scrollableElement_.addEventListener(
      'scroll', this.applyScrollClassTags_.bind(this));
    this.resizeObserver_.observe(this.scrollableElement_);
    for (let i = 0; i < sizeChangeObservableElemenets.length; ++i) {
      this.resizeObserver_.observe(sizeChangeObservableElemenets[i]);
    }
  },

  /**
   * Applies the class tags to topScrollContainer that control the shadows.
   */
  applyScrollClassTags_() {
    const el = this.scrollableElement_;
    el.classList.toggle('can-scroll', el.clientHeight < el.scrollHeight);
    el.classList.toggle('is-scrolled', el.scrollTop > 0);
    el.classList.toggle(
        'scrolled-to-bottom',
        el.scrollTop + el.clientHeight >= el.scrollHeight);
  },

  /**
   * Scroll to the bottom of scrollalbe element.
   */
  scrollToBottom() {
    this.scrollableElement_.scrollTop = this.scrollableElement_.scrollHeight;
  },
};

/**
 * TODO: Replace with an interface. b/24294625
 * @typedef {{
 *   initScrollableObservers: function()
 * }}
 */
OobeScrollableBehavior.Proto;

/** @interface */
export class OobeScrollableBehaviorInterface {
  initScrollableObservers(scrollableElement, ...sizeChangeObservableElemenets) {
  }
}
