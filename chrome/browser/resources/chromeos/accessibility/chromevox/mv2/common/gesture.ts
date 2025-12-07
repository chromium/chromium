// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines the gestures that ChromeVox can process.
 */

/**
 * Must match string values in ax::mojom::Gesture, defined in
 *     ui/accessibility/ax_enums.mojom.
 */
export enum Gesture {
  CLICK = 'click',
  SWIPE_DOWN_1 = 'swipeDown1',
  SWIPE_DOWN_2 = 'swipeDown2',
  SWIPE_DOWN_3 = 'swipeDown3',
  SWIPE_LEFT_1 = 'swipeLeft1',
  SWIPE_LEFT_2 = 'swipeLeft2',
  SWIPE_LEFT_3 = 'swipeLeft3',
  SWIPE_LEFT_4 = 'swipeLeft4',
  SWIPE_RIGHT_1 = 'swipeRight1',
  SWIPE_RIGHT_2 = 'swipeRight2',
  SWIPE_RIGHT_3 = 'swipeRight3',
  SWIPE_RIGHT_4 = 'swipeRight4',
  SWIPE_UP_1 = 'swipeUp1',
  SWIPE_UP_2 = 'swipeUp2',
  SWIPE_UP_3 = 'swipeUp3',
  TAP_2 = 'tap2',
  TAP_4 = 'tap4',
  TOUCH_EXPLORE = 'touchExplore',
}
