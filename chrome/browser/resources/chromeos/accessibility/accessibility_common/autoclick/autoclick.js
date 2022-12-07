// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventHandler} from '../../common/event_handler.js';

/**
 * The hex color for the focus rings.
 * @private {string}
 * @const
 */
const AUTOCLICK_FOCUS_RING_COLOR = '#aac9fa';

/**
 * The amount of time to wait before hiding the focus rings from the display.
 * @private {number}
 * @const
 */
const AUTOCLICK_FOCUS_RING_DISPLAY_TIME_MS = 250;

/**
 * Class to manage Automatic Clicks' interaction with the accessibility tree.
 */
export class Autoclick {
  constructor() {
    /**
     * Whether to blink the focus rings. Disabled during tests due to
     * complications with callbacks.
     * @private {boolean}
     */
    this.blinkFocusRings_ = true;

    /**
     * @private {chrome.automation.AutomationNode}
     */
    this.desktop_;

    /**
     * @private {?function(number, number)}
     */
    this.scrollableBoundsListener_ = null;

    /**
     * @private {!EventHandler}
     */
    this.hitTestHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_PRESSED,
        event => this.onAutomationHitTestResult_(event), {capture: true});

    /** @private {?function()} */
    this.onLoadDesktopCallbackForTest_ = null;

    this.init_();
  }

  setNoBlinkFocusRingsForTest() {
    this.blinkFocusRings_ = false;
  }

  /**
   * Destructor to remove any listeners.
   */
  onAutoclickDisabled() {
    if (this.scrollableBoundsListener_) {
      chrome.accessibilityPrivate.onScrollableBoundsForPointRequested
          .removeListener(this.scrollableBoundsListener_);
      this.scrollableBoundsListener_ = null;
    }

    this.hitTestHandler_.stop();
  }

  /**
   * Initializes Autoclick.
   * @private
   */
  init_() {
    this.scrollableBoundsListener_ = (x, y) =>
        this.findScrollingContainerForPoint_(x, y);

    chrome.automation.getDesktop(desktop => {
      this.desktop_ = desktop;

      // We use a hit test at a point to determine what automation node is
      // at that point, in order to find the scrollable area.
      this.hitTestHandler_.setNodes(this.desktop_);
      this.hitTestHandler_.start();

      if (this.onLoadDesktopCallbackForTest_) {
        this.onLoadDesktopCallbackForTest_();
        this.onLoadDesktopCallbackForTest_ = null;
      }
    });

    chrome.accessibilityPrivate.onScrollableBoundsForPointRequested.addListener(
        this.scrollableBoundsListener_);
  }

  /**
   * Sets the focus ring to |rects|.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects
   * @private
   */
  setFocusRings_(rects) {
    // TODO(katie): Add a property to FocusRingInfo to set FocusRingBehavior
    // to fade out.
    chrome.accessibilityPrivate.setFocusRings([{
      rects,
      type: chrome.accessibilityPrivate.FocusType.SOLID,
      color: AUTOCLICK_FOCUS_RING_COLOR,
      secondaryColor: AUTOCLICK_FOCUS_RING_COLOR,
    }]);
  }

  /**
   * Calculates whether a node should be highlighted as scrollable.
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean} True if the node should be highlighted as scrollable.
   * @private
   */
  shouldHighlightAsScrollable_(node) {
    if (node.scrollable === undefined || !node.scrollable) {
      return false;
    }

    // Check that the size of the scrollable area is larger than the node's
    // location. If it is not larger, then scrollbars are not shown.
    return node.scrollXMax - node.scrollXMin > node.location.width ||
        node.scrollYMax - node.scrollYMin > node.location.height;
  }

  /**
   * Processes an automation hit test result.
   * @param {!chrome.automation.AutomationEvent} event The hit test result
   *     event.
   * @private
   */
  onAutomationHitTestResult_(event) {
    // Walk up to the nearest scrollale area containing the point.
    let node = event.target;
    while (node.parent && node.role !== chrome.automation.RoleType.WINDOW &&
           node.role !== chrome.automation.RoleType.ROOT_WEB_AREA &&
           node.role !== chrome.automation.RoleType.DESKTOP &&
           node.role !== chrome.automation.RoleType.DIALOG &&
           node.role !== chrome.automation.RoleType.ALERT_DIALOG &&
           node.role !== chrome.automation.RoleType.TOOLBAR) {
      if (this.shouldHighlightAsScrollable_(node)) {
        break;
      }
      node = node.parent;
    }
    if (!node.location) {
      return;
    }
    const bounds = node.location;
    this.setFocusRings_([bounds]);
    if (this.blinkFocusRings_) {
      // Blink the focus ring briefly per UX spec, using timeouts to turn it
      // off, on, and off again. The focus ring is only used to show the user
      // where the scroll might occur, but is not persisted after the blink.
      // Turn off after 500 ms.
      setTimeout(() => {
        this.setFocusRings_([]);
      }, AUTOCLICK_FOCUS_RING_DISPLAY_TIME_MS * 2);
      // Back on after an additional 250 ms.
      setTimeout(() => {
        this.setFocusRings_([bounds]);
      }, AUTOCLICK_FOCUS_RING_DISPLAY_TIME_MS * 3);
      // And off after another 500 ms.
      setTimeout(() => {
        this.setFocusRings_([]);
      }, AUTOCLICK_FOCUS_RING_DISPLAY_TIME_MS * 5);
    }
    chrome.accessibilityPrivate.handleScrollableBoundsForPointFound(bounds);
  }

  /**
   * Initiates finidng the nearest scrolling container for the given point.
   * @param {number} x
   * @param {number} y
   * @private
   */
  findScrollingContainerForPoint_(x, y) {
    // The hit test will come back through onAutmoationHitTestResult_,
    // which will do the logic for finding the scrolling container.
    this.desktop_.hitTest(x, y, chrome.automation.EventType.MOUSE_PRESSED);
  }

  /**
   * Used by C++ tests to ensure Autoclick JS load is completed.
   * @param {!function()} callback Callback for when desktop is loaded from
   * automation.
   */
  setOnLoadDesktopCallbackForTest(callback) {
    if (!this.desktop_) {
      this.onLoadDesktopCallbackForTest_ = callback;
      return;
    }
    // Desktop already loaded.
    callback();
  }
}
