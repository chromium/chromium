// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Stores the current focus bounds and manages setting the focus
 * ring location.
 */

import {constants} from '/common/constants.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

export const FocusBounds = {
  /** @return {!Array<!chrome.accessibilityPrivate.ScreenRect>} */
  get() {
    return FocusBounds.current_;
  },

  /** @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} bounds */
  set(bounds) {
    FocusBounds.current_ = bounds;
    chrome.accessibilityPrivate.setFocusRings(
        [{
          rects: bounds,
          type: chrome.accessibilityPrivate.FocusType.GLOW,
          color: constants.FOCUS_COLOR,
        }],
        chrome.accessibilityPrivate.AssistiveTechnologyType.CHROME_VOX,
    );
  },
};

/** @private {!Array<!chrome.accessibilityPrivate.ScreenRect>} */
FocusBounds.current_ = [];

TestImportManager.exportForTesting(['FocusBounds', FocusBounds]);
