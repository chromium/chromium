// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

import ScreenPoint = chrome.accessibilityPrivate.ScreenPoint;
import ScreenRect = chrome.accessibilityPrivate.ScreenRect;
import ScrollDirection = chrome.accessibilityPrivate.ScrollDirection;

/** Handles all scroll interaction. */
export class ScrollModeController {
  private active_ = false;
  private scrollLocation_: ScreenPoint|undefined;
  private lastScrollTime_ = 0;
  private screenBounds_: ScreenRect|undefined;
  private originalCursorControlPref_: boolean|undefined;

  active(): boolean {
    return this.active_;
  }

  toggle(
      mouseLocation: ScreenPoint|undefined,
      screenBounds: ScreenRect|undefined): void {
    if (!mouseLocation || !screenBounds) {
      return;
    }

    this.active_ ? this.stop_() : this.start_(mouseLocation, screenBounds);
  }

  updateScrollLocation(mouseLocation: ScreenPoint|undefined): void {
    if (!mouseLocation) {
      return;
    }

    this.scrollLocation_ = mouseLocation;
  }

  private async start_(mouseLocation: ScreenPoint, screenBounds: ScreenRect):
      Promise<void> {
    this.active_ = true;
    this.scrollLocation_ = mouseLocation;
    this.screenBounds_ = screenBounds;
    chrome.settingsPrivate.getPref(
        ScrollModeController.PREF_CURSOR_CONTROL_ENABLED, pref => {
          // Save the original cursor control setting and ensure cursor control
          // is enabled.
          this.originalCursorControlPref_ = pref.value;
          chrome.settingsPrivate.setPref(
              ScrollModeController.PREF_CURSOR_CONTROL_ENABLED, true);
        });
  }

  private stop_(): void {
    this.active_ = false;
    this.scrollLocation_ = undefined;
    this.screenBounds_ = undefined;
    this.lastScrollTime_ = 0;

    // Set cursor control back to its original setting.
    chrome.settingsPrivate.setPref(
        ScrollModeController.PREF_CURSOR_CONTROL_ENABLED,
        Boolean(this.originalCursorControlPref_));
    this.originalCursorControlPref_ = undefined;
  }

  /** Scrolls based on the new mouse location. */
  scroll(mouseLocation: ScreenPoint): void {
    if (!this.active_ || !this.scrollLocation_ || !this.screenBounds_ ||
        (new Date().getTime() - this.lastScrollTime_ <
         ScrollModeController.RATE_LIMIT)) {
      return;
    }

    // To scroll, the user must move the mouse to one of the four edges of the
    // screen. We prioritize up and down scrolling because it's more common to
    // scroll in these directions. In the up and down directions, we provide
    // a cushion so that the mouse doesn't have to be exactly at the top or
    // bottom of the screen. This makes it easier to scroll up/down.
    const verticalCushion = this.screenBounds_.height *
        ScrollModeController.VERTICAL_CUSHION_FACTOR;
    let direction;
    if (mouseLocation.y <= this.screenBounds_.top + verticalCushion) {
      direction = ScrollDirection.UP;
    } else if (
        mouseLocation.y >=
        this.screenBounds_.height + this.screenBounds_.top - verticalCushion) {
      direction = ScrollDirection.DOWN;
    } else if (mouseLocation.x <= this.screenBounds_.left) {
      direction = ScrollDirection.LEFT;
    } else if (
        mouseLocation.x >= this.screenBounds_.width + this.screenBounds_.left) {
      direction = ScrollDirection.RIGHT;
    }

    if (!direction) {
      return;
    }

    this.lastScrollTime_ = new Date().getTime();
    chrome.accessibilityPrivate.scrollAtPoint(this.scrollLocation_, direction);
  }
}

export namespace ScrollModeController {
  export const PREF_CURSOR_CONTROL_ENABLED =
      'settings.a11y.face_gaze.cursor_control_enabled';
  // The time in milliseconds that needs to be exceeded before sending another
  // scroll.
  export const RATE_LIMIT = 50;
  // The amount of cushion provided at the top and bottom of the screen during
  // scroll mode.
  export const VERTICAL_CUSHION_FACTOR = 0.1;
}

TestImportManager.exportForTesting(ScrollModeController);
