// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRTL} from 'chrome://resources/js/util.m.js';

/**
 * Idle time in ms before the UI is hidden.
 * @type {number}
 */
const HIDE_TIMEOUT = 2000;

/**
 * Velocity required in a mousemove to reveal the UI (pixels/ms). This is
 * intended to be high enough that a fast flick of the mouse is required to
 * reach it.
 * @type {number}
 */
const SHOW_VELOCITY = 10;

/**
 * Distance from right of the screen required to reveal toolbars.
 * @type {number}
 */
const TOOLBAR_REVEAL_DISTANCE_RIGHT = 150;

/**
 * Distance from bottom of the screen required to reveal toolbars.
 * @type {number}
 */
const TOOLBAR_REVEAL_DISTANCE_BOTTOM = 250;

/**
 * @param {!MouseEvent} e Event to test.
 * @param {Window} window Window to test against.
 * @return {boolean} True if the mouse is close to the bottom-right of the
 * screen.
 */
function isMouseNearToolbar(e, window) {
  const atSide = isRTL() ?
      e.x > window.innerWidth - TOOLBAR_REVEAL_DISTANCE_RIGHT :
      e.x < TOOLBAR_REVEAL_DISTANCE_RIGHT;
  const atBottom = e.y > window.innerHeight - TOOLBAR_REVEAL_DISTANCE_BOTTOM;
  return atSide && atBottom;
}

// Responsible for showing and hiding the zoom toolbar.
export class ToolbarManager {
  /**
   * @param {!Window} window The window containing the UI.
   * @param {!ViewerZoomToolbarElement} zoomToolbar
   */
  constructor(window, zoomToolbar) {
    /** @private {!Window} */
    this.window_ = window;

    /** @private {!ViewerZoomToolbarElement} */
    this.zoomToolbar_ = zoomToolbar;

    /** @private {?number} */
    this.toolbarTimeout_ = null;

    /** @private {boolean} */
    this.isMouseNearToolbar_ = false;

    /** @private {boolean} */
    this.keyboardNavigationActive = false;

    /** @private {?number} */
    this.lastMovementTimestamp = null;

    document.addEventListener(
        'mousemove',
        e => this.handleMouseMove_(/** @type {!MouseEvent} */ (e)));
    document.addEventListener('mouseout', () => this.hideToolbarForMouseOut_());

    this.zoomToolbar_.addEventListener('keyboard-navigation-active', e => {
      this.keyboardNavigationActive = e.detail;
    });
  }

  /**
   * @param {!MouseEvent} e
   * @private
   */
  handleMouseMove_(e) {
    this.isMouseNearToolbar_ = isMouseNearToolbar(e, this.window_);

    this.keyboardNavigationActive = false;
    const touchInteractionActive =
        (e.sourceCapabilities && e.sourceCapabilities.firesTouchEvents);

    // Tapping the screen with toolbars open tries to close them.
    if (touchInteractionActive && this.zoomToolbar_.isVisible()) {
      this.hideToolbarIfAllowed_();
      return;
    }

    // Show the toolbars if the mouse is near the top or bottom-right of the
    // screen, if the mouse moved fast, or if the touchscreen was tapped.
    if (this.isMouseNearToolbar_ || this.isHighVelocityMouseMove_(e) ||
        touchInteractionActive) {
      this.zoomToolbar_.show();
    }
    this.hideToolbarAfterTimeout();
  }

  /**
   * Whether a mousemove event is high enough velocity to reveal the toolbars.
   * @param {!MouseEvent} e Event to test.
   * @return {boolean} true if the event is a high velocity mousemove, false
   * otherwise.
   * @private
   */
  isHighVelocityMouseMove_(e) {
    if (e.type === 'mousemove') {
      if (this.lastMovementTimestamp == null) {
        this.lastMovementTimestamp = this.getCurrentTimestamp_();
      } else {
        const movement =
            Math.sqrt(e.movementX * e.movementX + e.movementY * e.movementY);
        const newTime = this.getCurrentTimestamp_();
        const interval = newTime - this.lastMovementTimestamp;
        this.lastMovementTimestamp = newTime;

        if (interval !== 0) {
          return movement / interval > SHOW_VELOCITY;
        }
      }
    }
    return false;
  }

  /**
   * Wrapper around Date.now() to make it easily replaceable for testing.
   * @return {number}
   * @private
   */
  getCurrentTimestamp_() {
    return Date.now();
  }

  /**
   * Show toolbar and mark that navigation is being performed with
   * tab/shift-tab. This disables toolbar hiding until the mouse is moved or
   * escape is pressed.
   */
  showToolbarForKeyboardNavigation() {
    this.keyboardNavigationActive = true;
    this.zoomToolbar_.show();
  }

  /**
   * Hide toolbars after a delay, regardless of the position of the mouse.
   * Intended to be called when the mouse has moved out of the parent window.
   * @private
   */
  hideToolbarForMouseOut_() {
    this.isMouseNearToolbar_ = false;
    this.hideToolbarAfterTimeout();
  }

  /**
   * Check if the toolbar is able to be closed, and close it if it is.
   * Toolbar may be kept open based on mouse/keyboard activity and active
   * elements.
   * @private
   */
  hideToolbarIfAllowed_() {
    if (this.isMouseNearToolbar_ || this.keyboardNavigationActive) {
      return;
    }

    // Remove focus to make any visible tooltips disappear -- otherwise they'll
    // still be visible on screen when the toolbar is off screen.
    if (document.activeElement === this.zoomToolbar_) {
      document.activeElement.blur();
    }

    this.zoomToolbar_.hide();
  }

  /** Hide the toolbar after the HIDE_TIMEOUT has elapsed. */
  hideToolbarAfterTimeout() {
    if (this.toolbarTimeout_) {
      this.window_.clearTimeout(this.toolbarTimeout_);
    }
    this.toolbarTimeout_ = this.window_.setTimeout(
        this.hideToolbarIfAllowed_.bind(this), HIDE_TIMEOUT);
  }

  /**
   * Clears the keyboard navigation state and hides the toolbars after a delay.
   */
  resetKeyboardNavigationAndHideToolbar() {
    this.keyboardNavigationActive = false;
    this.hideToolbarAfterTimeout();
  }
}
