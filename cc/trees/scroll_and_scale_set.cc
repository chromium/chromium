// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/scroll_and_scale_set.h"

#include "cc/trees/swap_promise.h"

namespace cc {

ScrollAndScaleSet::ScrollAndScaleSet()
    : page_scale_delta(1.f),
      is_pinch_gesture_active(false),
      top_controls_delta(0.f),
      bottom_controls_delta(0.f),
      browser_controls_constraint(BrowserControlsState::kBoth),
      browser_controls_constraint_changed(false),
      scroll_gesture_did_end(false),
      manipulation_info(kManipulationInfoNone) {}

ScrollAndScaleSet::~ScrollAndScaleSet() = default;

}  // namespace cc
