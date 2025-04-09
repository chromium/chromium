// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventHandler} from '/common/event_handler.js';

/**
 * The hex color for the focus rings.
 */
const AUTOCLICK_FOCUS_RING_COLOR = '#aac9fa';

/**
 * The amount of time to wait before hiding the focus rings from the display.
 */
const AUTOCLICK_FOCUS_RING_DISPLAY_TIME_MS = 250;

/**
 * Class to manage Automatic Clicks' interaction with the accessibility tree.
 */
export class Autoclick {
  /**
   * Whether to blink the focus rings. Disabled during tests due to
   * complications with callbacks.
   */
  private blinkFocusRings_: boolean = true;

  private desktop_: chrome.automation.AutomationNode|null = null;

  private scrollableBoundsListener_:
      ((x: number, y: number) => void)|null = null;

  private hitTestHandler_: EventHandler;

  private onLoadDesktopCallbackForTest_: Function|null = null;


  constructor() {
    this.hitTestHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_PRESSED,
        event => this.onAutomationHitTestResult_(event), {
          capture: true,
          exactMatch: false,
          listenOnce: false,
          predicate: undefined,
        });

    this.init_();
  }

  setNoBlinkFocusRingsForTest(): void {
    this.blinkFocusRings_ = false;
  }

  /**
   * Destructor to remove any listeners.
   */
  onAutoclickDisabled(): void {
    if (this.scrollableBoundsListener_) {
      chrome.accessibilityPrivate.onScrollableBoundsForPointRequested
          .removeListener(this.scrollableBoundsListener_);
      this.scrollableBoundsListener_ = null;
    }

    this.hitTestHandler_.stop();
  }

  /**
   * Initializes Autoclick.
   */
  private init_(): void {
    this.scrollableBoundsListener_ = (x: number, y: number) =>
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
   */
  private setFocusRings_(rects: chrome.accessibilityPrivate.ScreenRect[]):
      void {
    // TODO(katie): Add a property to FocusRingInfo to set FocusRingBehavior
    // to fade out.
    chrome.accessibilityPrivate.setFocusRings(
        [{
          rects,
          type: chrome.accessibilityPrivate.FocusType.SOLID,
          color: AUTOCLICK_FOCUS_RING_COLOR,
          secondaryColor: AUTOCLICK_FOCUS_RING_COLOR,
        }],
        chrome.accessibilityPrivate.AssistiveTechnologyType.AUTO_CLICK);
  }

  /**
   * Calculates whether a node should be highlighted as scrollable.
   * Returns True  if the node should be highlighted as scrollable.
   */
  private shouldHighlightAsScrollable_(node: chrome.automation.AutomationNode):
      boolean {
    if (node.scrollable === undefined || !node.scrollable) {
      return false;
    }

    // Check that the size of the scrollable area is larger than the node's
    // location. If it is not larger, then scrollbars are not shown.
    return node.scrollXMax! - node.scrollXMin! > node.location.width ||
        node.scrollYMax! - node?.scrollYMin! > node.location.height;
  }

  /**
   * Processes an automation hit test result.
   */
  private onAutomationHitTestResult_(event: chrome.automation.AutomationEvent):
      void {
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
   * Initiates finding the nearest scrolling container for the given point.
   */
  private findScrollingContainerForPoint_(x: number, y: number): void {
    // The hit test will come back through onAutomationHitTestResult_,
    // which will do the logic for finding the scrolling container.
    this.desktop_!.hitTest(x, y, chrome.automation.EventType.MOUSE_PRESSED);
  }

  /**
   * Used by C++ tests to ensure Autoclick JS load is completed.
   * `callback` Callback for when desktop is loaded from
   * automation.
   */
  setOnLoadDesktopCallbackForTest(callback: Function): void {
    if (!this.desktop_) {
      this.onLoadDesktopCallbackForTest_ = callback;
      return;
    }
    // Desktop already loaded.
    callback();
  }
}
