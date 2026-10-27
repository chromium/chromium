// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_SNAP_HINT_H_
#define CC_INPUT_BROWSER_CONTROLS_SNAP_HINT_H_

#include "cc/cc_export.h"

namespace cc {

// Describes a browser controls snap animation hint produced by
// BrowserControlsSnapAnimationController.
struct CC_EXPORT BrowserControlsSnapHint {
  enum class Action {
    // Viewport is in the always-shown region; controls must be shown
    // regardless of scroll direction or threshold.
    kMustShow,
    // User scrolled up past the trigger threshold; controls may be shown.
    kCanShow,
    // User scrolled down past the trigger threshold in the can-hide region;
    // controls may be hidden.
    kCanHide,
  };

  Action action;
  // A factor between 0 and 1 based on how long it would take to animate the
  // controls to their final position given the current scroll velocity
  // compared to the maximum animation duration.
  float slowness = 1.0f;
  // True when the hint was produced on scroll end or during an inertial
  // scroll (i.e. after the user has lifted their finger).
  bool is_scroll_end_or_inertial = false;
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_SNAP_HINT_H_
