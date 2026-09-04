// Copyright 2018 The Chromium Authors
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
         delegated_ink_metadata == other.delegated_ink_metadata &&
         device_scale_factor == other.device_scale_factor &&
         viewport_size_in_pixels == other.viewport_size_in_pixels &&
         page_scale_factor == other.page_scale_factor &&
         external_page_scale_factor == other.external_page_scale_factor &&
         browser_controls_metadata == other.browser_controls_metadata &&
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
         min_page_scale_factor == other.min_page_scale_factor &&
         max_page_scale_factor == other.max_page_scale_factor &&
         root_overflow_y_hidden == other.root_overflow_y_hidden &&
         scrollable_viewport_size == other.scrollable_viewport_size &&
         root_layer_size == other.root_layer_size &&
         has_transparent_background == other.has_transparent_background &&
#endif
         tracked_element_rects == other.tracked_element_rects &&
         local_surface_id == other.local_surface_id &&
         new_vertical_scroll_direction == other.new_vertical_scroll_direction &&
         primary_main_frame_item_sequence_number ==
             other.primary_main_frame_item_sequence_number;
}

bool RenderFrameMetadata::operator!=(const RenderFrameMetadata& other) const {
  return !operator==(other);
}

bool BrowserControlsMetadata::operator==(
    const BrowserControlsMetadata& other) const {
  return top_controls_height == other.top_controls_height &&
         top_controls_shown_ratio == other.top_controls_shown_ratio
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
         && bottom_controls_height == other.bottom_controls_height &&
         bottom_controls_shown_ratio == other.bottom_controls_shown_ratio &&
         top_controls_min_height_offset ==
             other.top_controls_min_height_offset &&
         bottom_controls_min_height_offset ==
             other.bottom_controls_min_height_offset
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
      ;
}

bool BrowserControlsMetadata::RequiresNewLocalSurfaceId(
    const BrowserControlsMetadata& previous) const {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  return *this != previous;
#else
  if (top_controls_height != previous.top_controls_height ||
      bottom_controls_height != previous.bottom_controls_height) {
    return true;
  }

#if BUILDFLAG(IS_ANDROID)
  if (has_offset_tag) {
    return false;
  }

  // When the browser controls become locked, the browser will update the
  // offset tags, and also update the controls' offsets if they don't match
  // the current renderer scroll position. These updates result in a new
  // renderer frame, but sometimes it gets drawn before the browser frame
  // with the updated offsets arrives, which causes the controls to jump, so
  // we need a new surface id here to sync the updates.
  if (previous.has_offset_tag) {
    return true;
  }

  // If BCIV is enabled but there's no offset tags, it means the controls
  // aren't scrollable, and any movement of the controls is the result of
  // the browser updating their offsets and submitting a new browser frame.
  // We need a new surface id in this case, as this is identical to the
  // situation without BCIV.
#endif  // BUILDFLAG(IS_ANDROID)

  return top_controls_shown_ratio != previous.top_controls_shown_ratio ||
         bottom_controls_shown_ratio != previous.bottom_controls_shown_ratio;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

}  // namespace cc
