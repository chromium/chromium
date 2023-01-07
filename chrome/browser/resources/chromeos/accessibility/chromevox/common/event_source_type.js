// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Types available for tracking the current event source.
 */

/** @enum {string} */
export const EventSourceType = {
  NONE: 'none',
  BRAILLE_KEYBOARD: 'brailleKeyboard',
  STANDARD_KEYBOARD: 'standardKeyboard',
  TOUCH_GESTURE: 'touchGesture',
};
