// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {setNodeDisplay, setNodePosition} from './util.js';

/**
 * Base class to represent a "view". A view is an absolutely positioned box on
 * the page.
 */
export class View {
  constructor() {
    this.isVisible_ = true;
  }

  /**
   * Called to reposition the view on the page. Measurements are in pixels.
   */
  setGeometry(left, top, width, height) {
    this.left_ = left;
    this.top_ = top;
    this.width_ = width;
    this.height_ = height;
  }

  /**
   * Called to show/hide the view.
   */
  show(isVisible) {
    this.isVisible_ = isVisible;
  }

  isVisible() {
    return this.isVisible_;
  }

  /**
   * Method of the observer class.
   *
   * Called to check if an observer needs the data it is
   * observing to be actively updated.
   */
  isActive() {
    return this.isVisible();
  }

  getLeft() {
    return this.left_;
  }

  getTop() {
    return this.top_;
  }

  getWidth() {
    return this.width_;
  }

  getHeight() {
    return this.height_;
  }

  getRight() {
    return this.getLeft() + this.getWidth();
  }

  getBottom() {
    return this.getTop() + this.getHeight();
  }

  setParameters(params) {}

  /**
   * Called when loading a log file, after clearing all events, but before
   * loading the new ones.  |polledData| contains the data from all
   * PollableData helpers.  |tabData| contains the data for the particular
   * tab.  |logDump| is the entire log dump, which includes the other two
   * values.  It's included separately so most views don't have to depend on
   * its specifics.
   */
  onLoadLogStart(polledData, tabData, logDump) {}

  /**
   * Called as the final step of loading a log file.  Arguments are the same
   * as onLoadLogStart.  Returns true to indicate the tab should be shown,
   * false otherwise.
   */
  onLoadLogFinish(polledData, tabData, logDump) {
    return false;
  }
}

//-----------------------------------------------------------------------------

/**
 * DivView is an implementation of View that wraps a DIV.
 */
export class DivView extends View {
  constructor(divId) {
    // Call superclass's constructor.
    super();

    this.node_ = $(divId);
    if (!this.node_) {
      throw new Error('Element ' + divId + ' not found');
    }

    // Initialize the default values to those of the DIV.
    this.width_ = this.node_.offsetWidth;
    this.height_ = this.node_.offsetHeight;
    this.isVisible_ = this.node_.style.display !== 'none';
  }

  setGeometry(left, top, width, height) {
    super.setGeometry(this, left, top, width, height);

    this.node_.style.position = 'absolute';
    setNodePosition(this.node_, left, top, width, height);
  }

  show(isVisible) {
    super.show(isVisible);
    setNodeDisplay(this.node_, isVisible);
  }

  /**
   * Returns the wrapped DIV
   */
  getNode() {
    return this.node_;
  }
}

//-----------------------------------------------------------------------------

/**
 * Implementation of View that sizes its child to fit the entire window.
 *
 * @param {!View} childView The child view.
 */
export class WindowView extends View {
  constructor(childView) {
    // Call superclass's constructor.
    super();

    this.childView_ = childView;
    window.addEventListener('resize', this.resetGeometry.bind(this), true);
  }

  setGeometry(left, top, width, height) {
    super.setGeometry(this, left, top, width, height);
    this.childView_.setGeometry(left, top, width, height);
  }

  show() {
    super.show(isVisible);
    this.childView_.show(isVisible);
  }

  resetGeometry() {
    this.setGeometry(
        0, 0, document.documentElement.clientWidth,
        document.documentElement.clientHeight);
  }
}

/**
 * View that positions two views vertically. The top view should be
 * fixed-height, and the bottom view will fill the remainder of the space.
 *
 *  +-----------------------------------+
 *  |            topView                |
 *  +-----------------------------------+
 *  |                                   |
 *  |                                   |
 *  |                                   |
 *  |          bottomView               |
 *  |                                   |
 *  |                                   |
 *  |                                   |
 *  |                                   |
 *  +-----------------------------------+
 */
export class VerticalSplitView extends View {
  /**
   * @param {!View} topView The top view.
   * @param {!View} bottomView The bottom view.
   */
  constructor(topView, bottomView) {
    // Call superclass's constructor.
    super();

    this.topView_ = topView;
    this.bottomView_ = bottomView;
  }

  setGeometry(left, top, width, height) {
    super.setGeometry(this, left, top, width, height);

    const fixedHeight = this.topView_.getHeight();
    this.topView_.setGeometry(left, top, width, fixedHeight);

    this.bottomView_.setGeometry(
        left, top + fixedHeight, width, height - fixedHeight);
  }

  show(isVisible) {
    super.show(isVisible);

    this.topView_.show(isVisible);
    this.bottomView_.show(isVisible);
  }
}

/**
 * View that positions two views horizontally. The left view should be
 * fixed-width, and the right view will fill the remainder of the space.
 *
 *  +----------+--------------------------+
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  | leftView |       rightView          |
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  +----------+--------------------------+
 */
export class HorizontalSplitView extends View {
  /**
   * @param {!View} leftView The left view.
   * @param {!View} rightView The right view.
   */
  constructor(leftView, rightView) {
    // Call superclass's constructor.
    super();

    this.leftView_ = leftView;
    this.rightView_ = rightView;
  }

  setGeometry(left, top, width, height) {
    super.setGeometry(left, top, width, height);

    const fixedWidth = this.leftView_.getWidth();
    this.leftView_.setGeometry(left, top, fixedWidth, height);

    this.rightView_.setGeometry(
        left + fixedWidth, top, width - fixedWidth, height);
  }

  show(isVisible) {
    super.show(isVisible);

    this.leftView_.show(isVisible);
    this.rightView_.show(isVisible);
  }
}
