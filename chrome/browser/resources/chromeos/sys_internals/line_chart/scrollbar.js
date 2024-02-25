// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createElementWithClassName} from 'chrome://resources/ash/common/util.js';

/**
 * Create by |LineChart.LineChart|.
 * Create a dummy scrollbar to show the position of the line chart and to scroll
 * the line chart, so we can draw the visible part of the line chart only
 * instead of drawing the whole chart.
 * @const
 */
export class Scrollbar {
  constructor(/** function(): undefined */ callback) {
    /** @const {function(): undefined} - Handle the scrolling event. */
    this.callback_ = callback;

    /** @type {number} - The range the scrollbar can scroll. */
    this.range_ = 0;

    /** @type {number} - The current position of the scrollbar. */
    this.position_ = 0;

    /** @type {number} - The real width of this scrollbar, in pixels. */
    this.width_ = 0;

    /** @type {Element} - The outer div to show the scrollbar. */
    this.outerDiv_ =
        createElementWithClassName('div', 'horizontal-scrollbar-outer');
    this.outerDiv_.addEventListener('scroll', this.onScroll_.bind(this));

    /** @type {Element} - The inner div to make outer div scrollable. */
    this.innerDiv_ =
        createElementWithClassName('div', 'horizontal-scrollbar-inner');
    this.outerDiv_.appendChild(this.innerDiv_);
  }

  /**
   * Scrolling event handler.
   */
  onScroll_() {
    const /** number */ newPosition = this.outerDiv_.scrollLeft;
    if (newPosition === this.position_) {
      return;
    }
    this.position_ = newPosition;
    this.callback_();
  }

  /** @return {Element} */
  getRootDiv() {
    return this.outerDiv_;
  }

  /**
   * Return the height of scrollbar element.
   * @return {number}
   */
  getHeight() {
    return this.outerDiv_.offsetHeight;
  }

  /** @return {number} */
  getRange() {
    return this.range_;
  }

  /**
   * Position may be float point number because |document.scrollLeft| may be
   * float point number.
   * @return {number}
   */
  getPosition() {
    return Math.round(this.position_);
  }

  /**
   * Change the size of the outer div and update the scrollbar position.
   * @param {number} width
   */
  resize(width) {
    if (this.width_ === width) {
      return;
    }
    this.width_ = width;
    this.updateOuterDivWidth_();
  }

  updateOuterDivWidth_() {
    this.constructor.setNodeWidth(this.outerDiv_, this.width_);
  }

  /**
   * Set the scrollable range to |range|. Use the inner div's width to control
   * the scrollable range. If position go out of range after range update, set
   * it to the boundary value.
   * @param {number} range
   */
  setRange(range) {
    this.range_ = range;
    this.updateInnerDivWidth_();
    if (range < this.position_) {
      this.position_ = range;
      this.updateScrollbarPosition_();
    }
  }

  updateInnerDivWidth_() {
    const width = this.outerDiv_.clientWidth;
    this.constructor.setNodeWidth(this.innerDiv_, width + this.range_);
  }

  /**
   * @param {Element} node
   * @param {number} width
   */
  static setNodeWidth(node, width) {
    node.style.width = width + 'px';
  }

  /**
   * Set the scrollbar position to |position|. If the new position go out of
   * range, set it to the boundary value.
   * @param {number} position
   */
  setPosition(position) {
    const /** number */ newPosition =
        Math.max(0, Math.min(position, this.range_));
    this.position_ = newPosition;
    this.updateScrollbarPosition_();
  }

  /**
   * Update the scrollbar position via Javascript scrollbar api. Position may
   * not be the same value as what we assigned even if the value is in the
   * range. See crbug.com/760425.
   */
  updateScrollbarPosition_() {
    if (this.outerDiv_.scrollLeft === this.position_) {
      return;
    }
    this.outerDiv_.scrollLeft = this.position_;
  }

  /**
   * Return true if scrollbar is at the right edge of the chart.
   * @return {boolean}
   */
  isScrolledToRightEdge() {
    /* |scrollLeft| may become a float point number even if we set it to some
     * integer value. If the distance to the right edge less than 2 pixels, we
     * consider that it is scrolled to the right edge.
     */
    const scrollLeftErrorAmount = 2;
    return this.position_ + scrollLeftErrorAmount > this.range_;
  }

  /**
   * Scroll the scrollbar to the right edge.
   */
  scrollToRightEdge() {
    this.setPosition(this.range_);
  }
}
