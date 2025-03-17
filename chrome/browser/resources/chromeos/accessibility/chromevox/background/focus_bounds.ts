// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Stores the current focus bounds and manages setting the focus
 * ring location.
 */

import {constants} from '/common/constants.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

type ScreenRect = chrome.accessibilityPrivate.ScreenRect;

export class FocusBounds {
  private static current_: ScreenRect[] = [];

  static get(): ScreenRect[] {
    return FocusBounds.current_;
  }

  static set(bounds: ScreenRect[]) {
    FocusBounds.current_ = bounds;
    chrome.accessibilityPrivate.setFocusRings(
        [{
          rects: bounds,
          type: chrome.accessibilityPrivate.FocusType.GLOW,
          color: constants.FOCUS_COLOR,
        }],
        chrome.accessibilityPrivate.AssistiveTechnologyType.CHROME_VOX,
    );
  }
}

TestImportManager.exportForTesting(['FocusBounds', FocusBounds]);
