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
  private mouseLocation_: ScreenPoint|undefined;
  private center_: ScreenPoint|undefined;
  private lastScrollTime_ = 0;

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
    this.active_ = !this.active_;
  }

  private async start_(mouseLocation: ScreenPoint, screenBounds: ScreenRect):
      Promise<void> {
    this.mouseLocation_ = mouseLocation;
    this.center_ = {
      x: Math.round(screenBounds.width / 2) + screenBounds.left,
      y: Math.round(screenBounds.height / 2) + screenBounds.top,
    };
  }

  private stop_(): void {
    this.mouseLocation_ = undefined;
    this.center_ = undefined;
    this.lastScrollTime_ = 0;
  }

  /** Scrolls based on the new mouse location. */
  scroll(newLocation: ScreenPoint): void {
    if (!this.active_ || !this.mouseLocation_ ||
        (new Date().getTime() - this.lastScrollTime_ <
         ScrollModeController.RATE_LIMIT)) {
      return;
    }

    const direction = this.getDirection_(newLocation);
    if (!direction) {
      return;
    }

    this.lastScrollTime_ = new Date().getTime();
    chrome.accessibilityPrivate.scrollAtPoint(this.mouseLocation_, direction);
  }

  private getDirection_(newLocation: ScreenPoint): ScrollDirection|undefined {
    if (!this.active_ || !this.center_) {
      return;
    }

    // Returns the distance between two points.
    const getDistance = (location: ScreenPoint, other: ScreenPoint): number => {
      return Math.sqrt(
          Math.pow(location.x - other.x, 2) +
          Math.pow(location.y - other.y, 2));
    };

    if (getDistance(this.center_, newLocation) <=
        ScrollModeController.DELTA_THRESHOLD) {
      // Don't scroll if the delta threshold isn't met. For example, we
      // don't want to scroll if the user's head is pointing ever-so-slightly
      // downward.
      return;
    }

    // Determines the angle between the positive x-axis and the provided
    // location. Return values range from [-180, 180].
    const getAngleDegrees = (location: ScreenPoint): number => {
      return (Math.atan2(location.y, location.x) * 180) / Math.PI;
    };

    // Translate newLocation to a coordinate system where this.center_ is
    // (0, 0). Note that newLocation is on a coordinate system where the top
    // left corner is (0,0) and the bottom right corner is (x-max, y-max). So
    // the y-coordinate needs to be flipped during translation.
    const translatedLocation = {
      x: newLocation.x - this.center_.x,
      y: (newLocation.y - this.center_.y) * -1,
    };

    // Determine the scroll direction based on the angle.
    let direction;
    const angle = getAngleDegrees(translatedLocation);
    // Check the counter-clockwise direction first.
    if (angle >= 0 && angle < 45) {
      direction = ScrollDirection.RIGHT;
    } else if (angle > 45 && angle < 135) {
      direction = ScrollDirection.UP;
    } else if (angle > 135 && angle <= 180) {
      direction = ScrollDirection.LEFT;
    }

    // Next check the clockwise direction.
    if (angle <= 0 && angle > -45) {
      direction = ScrollDirection.RIGHT;
    } else if (angle < -45 && angle > -135) {
      direction = ScrollDirection.DOWN;
    } else if (angle < -135 && angle >= -180) {
      direction = ScrollDirection.LEFT;
    }

    return direction;
  }
}

export namespace ScrollModeController {
  // The delta, in pixels, that needs to be exceeded for scrolling to be
  // triggered.
  export const DELTA_THRESHOLD = 100;
  // The time in milliseconds that needs to be exceeded before sending another
  // scroll.
  export const RATE_LIMIT = 250;
}

TestImportManager.exportForTesting(ScrollModeController);
