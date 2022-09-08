// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/browser_controls_params.h"

namespace cc {

bool BrowserControlsParams::operator==(
    const BrowserControlsParams& other) const {
  return top_controls_height == other.top_controls_height &&
         top_controls_min_height == other.top_controls_min_height &&
         bottom_controls_height == other.bottom_controls_height &&
         bottom_controls_min_height == other.bottom_controls_min_height &&
         animate_browser_controls_height_changes ==
             other.animate_browser_controls_height_changes &&
         browser_controls_shrink_blink_size ==
             other.browser_controls_shrink_blink_size &&
         only_expand_top_controls_at_page_top ==
             other.only_expand_top_controls_at_page_top;
}

bool BrowserControlsParams::operator!=(
    const BrowserControlsParams& other) const {
  return !(*this == other);
}

}  // namespace cc
