// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/render_frame_metadata.h"

#include "build/build_config.h"

namespace cc {

RenderFrameMetadata::RenderFrameMetadata() = default;

RenderFrameMetadata::RenderFrameMetadata(const RenderFrameMetadata& other) =
    default;

RenderFrameMetadata::RenderFrameMetadata(RenderFrameMetadata&& other) = default;

RenderFrameMetadata::~RenderFrameMetadata() {}

RenderFrameMetadata& RenderFrameMetadata::operator=(
    const RenderFrameMetadata&) = default;

RenderFrameMetadata& RenderFrameMetadata::operator=(
    RenderFrameMetadata&& other) = default;

bool RenderFrameMetadata::operator==(const RenderFrameMetadata& other) const {
  return root_scroll_offset == other.root_scroll_offset &&
         root_background_color == other.root_background_color &&
         is_scroll_offset_at_top == other.is_scroll_offset_at_top &&
         selection == other.selection &&
         is_mobile_optimized == other.is_mobile_optimized &&
         device_scale_factor == other.device_scale_factor &&
         viewport_size_in_pixels == other.viewport_size_in_pixels &&
         page_scale_factor == other.page_scale_factor &&
         external_page_scale_factor == other.external_page_scale_factor &&
         top_controls_height == other.top_controls_height &&
         top_controls_shown_ratio == other.top_controls_shown_ratio &&
#if defined(OS_ANDROID)
         bottom_controls_height == other.bottom_controls_height &&
         bottom_controls_shown_ratio == other.bottom_controls_shown_ratio &&
         min_page_scale_factor == other.min_page_scale_factor &&
         max_page_scale_factor == other.max_page_scale_factor &&
         root_overflow_y_hidden == other.root_overflow_y_hidden &&
         scrollable_viewport_size == other.scrollable_viewport_size &&
         root_layer_size == other.root_layer_size &&
         has_transparent_background == other.has_transparent_background &&
#endif
         local_surface_id_allocation == other.local_surface_id_allocation &&
         new_vertical_scroll_direction == other.new_vertical_scroll_direction;
}

bool RenderFrameMetadata::operator!=(const RenderFrameMetadata& other) const {
  return !operator==(other);
}

}  // namespace cc
