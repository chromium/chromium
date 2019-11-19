// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRTL} from 'chrome://resources/js/util.m.js';

/** Idle time in ms before the UI is hidden. */
const HIDE_TIMEOUT = 2000;
/** Time in ms after force hide before toolbar is shown again. */
const FORCE_HIDE_TIMEOUT = 1000;
/**
 * Velocity required in a mousemove to reveal the UI (pixels/ms). This is
 * intended to be high enough that a fast flick of the mouse is required to
 * reach it.
 */
const SHOW_VELOCITY = 10;
/** Distance from the top of the screen required to reveal the toolbars. */
const TOP_TOOLBAR_REVEAL_DISTANCE = 100;
/** Distance from the bottom-right of the screen required to reveal toolbars. */
const SIDE_TOOLBAR_REVEAL_DISTANCE_RIGHT = 150;
const SIDE_TOOLBAR_REVEAL_DISTANCE_BOTTOM = 250;

/**
 * @param {!MouseEvent} e Event to test.
 * @return {boolean} True if the mouse is close to the top of the screen.
 */
function isMouseNearTopToolbar(e) {
  return e.y < TOP_TOOLBAR_REVEAL_DISTANCE;
}

/**
 * @param {!MouseEvent} e Event to test.
 * @param {Window} window Window to test against.
 * @param {boolean} reverse Whether the side toolbar is reversed.
 * @return {boolean} True if the mouse is close to the bottom-right of the
 * screen.
 */
function isMouseNearSideToolbar(e, window, reverse) {
  let atSide = e.x > window.innerWidth - SIDE_TOOLBAR_REVEAL_DISTANCE_RIGHT;
  if (isRTL() !== reverse) {
    atSide = e.x < SIDE_TOOLBAR_REVEAL_DISTANCE_RIGHT;
  }
  const atBottom =
      e.y > window.innerHeight - SIDE_TOOLBAR_REVEAL_DISTANCE_BOTTOM;
  return atSide && atBottom;
}

/** Responsible for co-ordinating between multiple toolbar elements. */
export class ToolbarManager {
  /**
   * @param {!Window} window The window containing the UI.
   * @param {?ViewerPdfToolbarElement} toolbar
   * @param {!ViewerZoomToolbarElement} zoomToolbar
   */
  constructor(window, toolbar, zoomToolbar) {
    /** @private {!Window} */
    this.window_ = window;

    /** @private {?ViewerPdfToolbarElement} */
    this.toolbar_ = toolbar;

    /** @private {!ViewerZoomToolbarElement} */
    this.zoomToolbar_ = zoomToolbar;

    /** @private {?number} */
    this.toolbarTimeout_ = null;

    /** @private {boolean} */
    this.isMouseNearTopToolbar_ = false;

    /** @private {boolean} */
    this.isMouseNearSideToolbar_ = false;

    /** @private {boolean} */
    this.sideToolbarAllowedOnly_ = false;

    /** @private {?number} */
    this.sideToolbarAllowedOnlyTimer_ = null;

    /** @private {boolean} */
    this.keyboardNavigationActive = false;

    /** @private {?number} */
    this.lastMovementTimestamp = null;

    /** @private {boolean} */
    this.isPrintPreview_ = zoomToolbar.isPrintPreview;

    this.window_.addEventListener('resize', this.resizeDropdowns_.bind(this));
    this.resizeDropdowns_();

    if (this.isPrintPreview_) {
      this.zoomToolbar_.addEventListener('keyboard-navigation-active', e => {
        this.keyboardNavigationActive = e.detail;
      });
    }
  }

  /** @param {!MouseEvent} e */
  handleMouseMove(e) {
    this.isMouseNearTopToolbar_ = !!this.toolbar_ && isMouseNearTopToolbar(e);
    this.isMouseNearSideToolbar_ =
        isMouseNearSideToolbar(e, this.window_, this.isPrintPreview_);

    this.keyboardNavigationActive = false;
    const touchInteractionActive =
        (e.sourceCapabilities && e.sourceCapabilities.firesTouchEvents);

    // Allow the top toolbar to be shown if the mouse moves away from the side
    // toolbar (as long as the timeout has elapsed).
    if (!this.isMouseNearSideToolbar_ && !this.sideToolbarAllowedOnlyTimer_) {
      this.sideToolbarAllowedOnly_ = false;
    }

    // Allow the top toolbar to be shown if the mouse moves to the top edge.
    if (this.isMouseNearTopToolbar_) {
      this.sideToolbarAllowedOnly_ = false;
    }

    // Tapping the screen with toolbars open tries to close them.
    if (touchInteractionActive && this.zoomToolbar_.isVisible()) {
      this.hideToolbarsIfAllowed();
      return;
    }

    // Show the toolbars if the mouse is near the top or bottom-right of the
    // screen, if the mouse moved fast, or if the touchscreen was tapped.
    if (this.isMouseNearTopToolbar_ || this.isMouseNearSideToolbar_ ||
        this.isHighVelocityMouseMove_(e) || touchInteractionActive) {
      if (this.sideToolbarAllowedOnly_) {
        this.zoomToolbar_.show();
      } else {
        this.showToolbars();
      }
    }
    this.hideToolbarsAfterTimeout();
  }

  /**
   * Whether a mousemove event is high enough velocity to reveal the toolbars.
   * @param {!MouseEvent} e Event to test.
   * @return {boolean} true if the event is a high velocity mousemove, false
   * otherwise.
   * @private
   */
  isHighVelocityMouseMove_(e) {
    if (e.type == 'mousemove') {
      if (this.lastMovementTimestamp == null) {
        this.lastMovementTimestamp = this.getCurrentTimestamp_();
      } else {
        const movement =
            Math.sqrt(e.movementX * e.movementX + e.movementY * e.movementY);
        const newTime = this.getCurrentTimestamp_();
        const interval = newTime - this.lastMovementTimestamp;
        this.lastMovementTimestamp = newTime;

        if (interval != 0) {
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

  /** Display both UI toolbars. */
  showToolbars() {
    if (this.toolbar_) {
      this.toolbar_.show();
    }
    this.zoomToolbar_.show();
  }

  /**
   * Show toolbars and mark that navigation is being performed with
   * tab/shift-tab. This disables toolbar hiding until the mouse is moved or
   * escape is pressed.
   */
  showToolbarsForKeyboardNavigation() {
    this.keyboardNavigationActive = true;
    this.showToolbars();
  }

  /**
   * Hide toolbars after a delay, regardless of the position of the mouse.
   * Intended to be called when the mouse has moved out of the parent window.
   */
  hideToolbarsForMouseOut() {
    this.isMouseNearTopToolbar_ = false;
    this.isMouseNearSideToolbar_ = false;
    this.hideToolbarsAfterTimeout();
  }

  /**
   * Check if the toolbars are able to be closed, and close them if they are.
   * Toolbars may be kept open based on mouse/keyboard activity and active
   * elements.
   */
  hideToolbarsIfAllowed() {
    if (this.isMouseNearSideToolbar_ || this.isMouseNearTopToolbar_) {
      return;
    }

    if (this.toolbar_ && this.toolbar_.shouldKeepOpen()) {
      return;
    }

    if (this.keyboardNavigationActive) {
      return;
    }

    // Remove focus to make any visible tooltips disappear -- otherwise they'll
    // still be visible on screen when the toolbar is off screen.
    if ((this.toolbar_ && document.activeElement == this.toolbar_) ||
        document.activeElement == this.zoomToolbar_) {
      document.activeElement.blur();
    }

    if (this.toolbar_) {
      this.toolbar_.hide();
    }
    this.zoomToolbar_.hide();
  }

  /** Hide the toolbars after the HIDE_TIMEOUT has elapsed. */
  hideToolbarsAfterTimeout() {
    if (this.toolbarTimeout_) {
      this.window_.clearTimeout(this.toolbarTimeout_);
    }
    this.toolbarTimeout_ = this.window_.setTimeout(
        this.hideToolbarsIfAllowed.bind(this), HIDE_TIMEOUT);
  }

  /**
   * Hide the 'topmost' layer of toolbars. Hides any dropdowns that are open, or
   * hides the basic toolbars otherwise.
   */
  hideSingleToolbarLayer() {
    if (!this.toolbar_ || !this.toolbar_.hideDropdowns()) {
      this.keyboardNavigationActive = false;
      this.hideToolbarsIfAllowed();
    }
  }

  /**
   * Clears the keyboard navigation state and hides the toolbars after a delay.
   */
  resetKeyboardNavigationAndHideToolbars() {
    this.keyboardNavigationActive = false;
    this.hideToolbarsAfterTimeout();
  }

  /**
   * Hide the top toolbar and keep it hidden until both:
   * - The mouse is moved away from the right side of the screen
   * - 1 second has passed.
   * The top toolbar can be immediately re-opened by moving the mouse to the top
   * of the screen.
   */
  forceHideTopToolbar() {
    if (!this.toolbar_) {
      return;
    }
    this.toolbar_.hide();
    this.sideToolbarAllowedOnly_ = true;
    this.sideToolbarAllowedOnlyTimer_ = this.window_.setTimeout(() => {
      this.sideToolbarAllowedOnlyTimer_ = null;
    }, FORCE_HIDE_TIMEOUT);
  }

  /**
   * Updates the size of toolbar dropdowns based on the positions of the rest of
   * the UI.
   * @private
   */
  resizeDropdowns_() {
    if (!this.toolbar_) {
      return;
    }
    const lowerBound =
        this.window_.innerHeight - this.zoomToolbar_.clientHeight;
    this.toolbar_.setDropdownLowerBound(lowerBound);
  }
}
