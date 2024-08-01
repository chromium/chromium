// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRTL} from 'chrome://resources/js/util.js';

import type {ViewerZoomToolbarElement} from './elements/viewer_zoom_toolbar.js';

/**
 * Idle time in ms before the UI is hidden.
 */
const HIDE_TIMEOUT: number = 2000;

/**
 * Velocity required in a mousemove to reveal the UI (pixels/ms). This is
 * intended to be high enough that a fast flick of the mouse is required to
 * reach it.
 */
const SHOW_VELOCITY: number = 10;

/**
 * Distance from right of the screen required to reveal toolbars.
 */
const TOOLBAR_REVEAL_DISTANCE_RIGHT: number = 150;

/**
 * Distance from bottom of the screen required to reveal toolbars.
 */
const TOOLBAR_REVEAL_DISTANCE_BOTTOM: number = 250;

/**
 * @param e Event to test.
 * @param window Window to test against.
 * @return True if the mouse is close to the bottom-right of the screen.
 */
function isMouseNearToolbar(e: MouseEvent, window: Window): boolean {
  const atSide = isRTL() ?
      e.x > window.innerWidth - TOOLBAR_REVEAL_DISTANCE_RIGHT :
      e.x < TOOLBAR_REVEAL_DISTANCE_RIGHT;
  const atBottom = e.y > window.innerHeight - TOOLBAR_REVEAL_DISTANCE_BOTTOM;
  return atSide && atBottom;
}

// Responsible for showing and hiding the zoom toolbar.
export class ToolbarManager {
  private window_: Window;
  private zoomToolbar_: ViewerZoomToolbarElement;
  private toolbarTimeout_: number|null = null;
  private isMouseNearToolbar_: boolean = false;
  private keyboardNavigationActive_: boolean = false;
  private lastMovementTimestamp_: number|null = null;

  /**
   * @param window The window containing the UI.
   */
  constructor(window: Window, zoomToolbar: ViewerZoomToolbarElement) {
    this.window_ = window;
    this.zoomToolbar_ = zoomToolbar;

    document.addEventListener('mousemove', e => this.handleMouseMove_(e));
    document.addEventListener('mouseout', () => this.hideToolbarForMouseOut_());

    this.zoomToolbar_.addEventListener('keyboard-navigation-active', e => {
      this.keyboardNavigationActive_ = (e as CustomEvent).detail;
    });
  }

  private handleMouseMove_(e: MouseEvent) {
    this.isMouseNearToolbar_ = isMouseNearToolbar(e, this.window_);

    this.keyboardNavigationActive_ = false;

    const touchInteractionActive =
        e.sourceCapabilities && e.sourceCapabilities.firesTouchEvents;

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
   */
  private isHighVelocityMouseMove_(e: MouseEvent): boolean {
    if (e.type === 'mousemove') {
      if (this.lastMovementTimestamp_ == null) {
        this.lastMovementTimestamp_ = this.getCurrentTimestamp();
      } else {
        const movement =
            Math.sqrt(e.movementX * e.movementX + e.movementY * e.movementY);
        const newTime = this.getCurrentTimestamp();
        const interval = newTime - this.lastMovementTimestamp_;
        this.lastMovementTimestamp_ = newTime;

        if (interval !== 0) {
          return movement / interval > SHOW_VELOCITY;
        }
      }
    }
    return false;
  }

  /**
   * Wrapper around Date.now() to make it easily replaceable for testing.
   */
  getCurrentTimestamp(): number {
    return Date.now();
  }

  /**
   * Show toolbar and mark that navigation is being performed with
   * tab/shift-tab. This disables toolbar hiding until the mouse is moved or
   * escape is pressed.
   */
  showToolbarForKeyboardNavigation() {
    this.keyboardNavigationActive_ = true;
    this.zoomToolbar_.show();
  }

  /**
   * Hide toolbars after a delay, regardless of the position of the mouse.
   * Intended to be called when the mouse has moved out of the parent window.
   */
  private hideToolbarForMouseOut_() {
    this.isMouseNearToolbar_ = false;
    this.hideToolbarAfterTimeout();
  }

  /**
   * Check if the toolbar is able to be closed, and close it if it is.
   * Toolbar may be kept open based on mouse/keyboard activity and active
   * elements.
   */
  private hideToolbarIfAllowed_() {
    if (this.isMouseNearToolbar_ || this.keyboardNavigationActive_) {
      return;
    }

    // Remove focus to make any visible tooltips disappear -- otherwise they'll
    // still be visible on screen when the toolbar is off screen.
    if (document.activeElement === this.zoomToolbar_) {
      this.zoomToolbar_.blur();
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
    this.keyboardNavigationActive_ = false;
    this.hideToolbarAfterTimeout();
  }
}
