// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_MOBILE_OPTIMIZED_VIEWPORT_UTIL_H_
#define CC_TREES_MOBILE_OPTIMIZED_VIEWPORT_UTIL_H_

#include "ui/gfx/geometry/size_f.h"

namespace cc {
namespace util {

// Returns whether the viewport should be considered mobile optimized,
// not needing the double tap to zoom gesture.
// Arguments:
// min_page_scale_factor - the minimum page scale
// max_page_scale_factor - the maximum page scale
// current_page_scale_factor - current page scale
// scrollable_viewport_size - the size of the user-visible scrolling viewport
// in CSS layout coordinates
// scrollable_size - the size of the root scrollable area in CSS layout
// coordinates
// viewport_meta_mobile_optimized - if the viewport meta tag is mobile
// optimized
bool IsMobileOptimized(float min_page_scale_factor,
                       float max_page_scale_factor,
                       float current_page_scale_factor,
                       gfx::SizeF scrollable_viewport_size,
                       gfx::SizeF scrollable_size,
                       bool viewport_meta_mobile_optimized);
}  // namespace util
}  // namespace cc

#endif  // CC_TREES_MOBILE_OPTIMIZED_VIEWPORT_UTIL_H_
