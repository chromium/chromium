// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/mobile_optimized_viewport_util.h"

#include "base/feature_list.h"
#include "cc/base/features.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {
namespace util {
namespace {
// Used to accommodate finite precision when comparing scaled viewport and
// content widths. While this value may seem large, width=device-width on an N7
// V1 saw errors of ~0.065 between computed window and content widths.
const float kMobileViewportWidthEpsilon = 0.15f;
}  // namespace

bool IsMobileOptimized(float min_page_scale_factor,
                       float max_page_scale_factor,
                       float current_page_scale_factor,
                       gfx::SizeF scrollable_viewport_size,
                       gfx::SizeF scrollable_size,
                       bool viewport_meta_mobile_optimized) {
  bool has_fixed_page_scale = min_page_scale_factor == max_page_scale_factor;

  float window_width_dip =
      current_page_scale_factor * scrollable_viewport_size.width();
  float content_width_css = scrollable_size.width();
  bool has_mobile_viewport =
      content_width_css <= window_width_dip + kMobileViewportWidthEpsilon;

  return has_mobile_viewport || has_fixed_page_scale ||
         viewport_meta_mobile_optimized;
}

}  // namespace util
}  // namespace cc
