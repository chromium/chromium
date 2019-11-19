// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * A mock AccessibilityPrivate API for tests.
 */
var MockAccessibilityPrivate = {
  FocusType: {
    SOLID: 'solid',
  },

  /** @private {function<number, number>} */
  boundsListener_: null,

  /** @private {!chrome.accessibilityPrivate.ScreenRect} */
  scrollableBounds_: {},

  /** @private {!Array<!chrome.accessibilityPrivate.ScreenRect>} */
  focusRingRects_: [],
  onScrollableBoundsForPointFoundCallback_: null,

  // Methods from AccessibilityPrivate API. //

  findScrollableBoundsForPoint: {
    /**
     * Adds a listener to findScrollableBoundsForPoint.
     * @param {function<number, number>} listener
     */
    addListener: (listener) => {
      boundsListener_ = listener;
    }
  },

  /**
   * Called when Autoclick finds scrollable bounds at a point.
   * @param {!chrome.accessibilityPrivate.ScreenRect} bounds
   */
  onScrollableBoundsForPointFound: (bounds) => {
    scrollableBounds_ = bounds;
    onScrollableBoundsForPointFoundCallback_();
  },

  /**
   * Called when Autoclick wants to set the focus rings. We can assume that
   * it is only setting one set of rings at a time, and safely extract
   * focusRingInfos[0].rects.
   * @param {!Array<!FocusRingInfo>} focusRingInfos
   */
  setFocusRings: (focusRingInfos) => {
    focusRingRects_ = focusRingInfos[0].rects;
  },

  // Methods for testing. //

  /**
   * Called to get the autoclick extension to use the Automation API to find
   * the scrollable bounds at a point. In Automatic Clicks, this would actually
   * be initiated by ash/autoclick/autoclick_controller calling the
   * AccessibilityPrivate API call.
   * When the bounds are found, onScrollableBoundsForPointFoundCallback will
   * be called to inform the test that work is complete.
   * @param {number} x
   * @param {number} y
   * @param {!function<>} onScrollableBoundsForPointFoundCallback
   */
  callFindScrollableBoundsForPoint:
      (x, y, onScrollableBoundsForPointFoundCallback) => {
        onScrollableBoundsForPointFoundCallback_ =
            onScrollableBoundsForPointFoundCallback;
        boundsListener_(x, y);
      },

  /**
   * Gets the scrollable bounds which were found by the Autoclick extension.
   * @return {Array<!chrome.AccessibilityPrivate.ScreenRect>}
   */
  getScrollableBounds: () => {
    return scrollableBounds_;
  },

  /**
   * Gets the focus rings bounds which were set by the Autoclick extension.
   * @return {Array<!chrome.AccessibilityPrivate.ScreenRect>}
   */
  getFocusRings: () => {
    return focusRingRects_;
  },
};