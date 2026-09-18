// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojom/render_frame_metadata_mojom_traits.h"

#include <cmath>
#include <string_view>

#include "base/debug/crash_logging.h"
#include "build/build_config.h"
#include "services/viz/public/cpp/compositing/selection_mojom_traits.h"
#include "services/viz/public/cpp/compositing/tracked_element_rects_mojom_traits.h"
#include "services/viz/public/cpp/compositing/vertical_scroll_direction_mojom_traits.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/selection_bound_mojom_traits.h"

namespace mojo {

namespace {

void SetFailedCheckCrashKey(std::string_view check_name) {
  static auto* const crash_key = base::debug::AllocateCrashKeyString(
      "rfm_failed_check", base::debug::CrashKeySize::Size64);
  base::debug::SetCrashKeyString(crash_key, check_name);
}

}  // namespace

// static
bool StructTraits<cc::mojom::DelegatedInkBrowserMetadataDataView,
                  cc::DelegatedInkBrowserMetadata>::
    Read(cc::mojom::DelegatedInkBrowserMetadataDataView data,
         cc::DelegatedInkBrowserMetadata* out) {
  out->delegated_ink_is_hovering = data.delegated_ink_is_hovering();
  return true;
}

// static
bool StructTraits<cc::mojom::BrowserControlsMetadataDataView,
                  cc::BrowserControlsMetadata>::
    Read(cc::mojom::BrowserControlsMetadataDataView data,
         cc::BrowserControlsMetadata* out) {
  // Validate every field before touching `out`, so that a rejected message
  // never leaves the output struct partially populated with untrusted values.
  const float top_controls_height = data.top_controls_height();
  if (!std::isfinite(top_controls_height) || top_controls_height < 0.f) {
    SetFailedCheckCrashKey("top_controls_height");
    return false;
  }
  const float top_controls_shown_ratio = data.top_controls_shown_ratio();
  if (!std::isfinite(top_controls_shown_ratio) ||
      top_controls_shown_ratio < 0.f || top_controls_shown_ratio > 1.f) {
    SetFailedCheckCrashKey("top_controls_shown_ratio");
    return false;
  }
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const float bottom_controls_height = data.bottom_controls_height();
  if (!std::isfinite(bottom_controls_height) || bottom_controls_height < 0.f) {
    SetFailedCheckCrashKey("bottom_controls_height");
    return false;
  }
  const float bottom_controls_shown_ratio = data.bottom_controls_shown_ratio();
  if (!std::isfinite(bottom_controls_shown_ratio) ||
      bottom_controls_shown_ratio < 0.f || bottom_controls_shown_ratio > 1.f) {
    SetFailedCheckCrashKey("bottom_controls_shown_ratio");
    return false;
  }
  const float top_controls_min_height_offset =
      data.top_controls_min_height_offset();
  if (!std::isfinite(top_controls_min_height_offset) ||
      top_controls_min_height_offset < 0.f) {
    SetFailedCheckCrashKey("top_controls_min_height_offset");
    return false;
  }
  const float bottom_controls_min_height_offset =
      data.bottom_controls_min_height_offset();
  if (!std::isfinite(bottom_controls_min_height_offset) ||
      bottom_controls_min_height_offset < 0.f) {
    SetFailedCheckCrashKey("bottom_controls_min_height_offset");
    return false;
  }
#endif

  out->top_controls_height = top_controls_height;
  out->top_controls_shown_ratio = top_controls_shown_ratio;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  out->bottom_controls_height = bottom_controls_height;
  out->bottom_controls_shown_ratio = bottom_controls_shown_ratio;
  out->top_controls_min_height_offset = top_controls_min_height_offset;
  out->bottom_controls_min_height_offset = bottom_controls_min_height_offset;
#endif

  return true;
}

// static
bool StructTraits<
    cc::mojom::RenderFrameMetadataDataView,
    cc::RenderFrameMetadata>::Read(cc::mojom::RenderFrameMetadataDataView data,
                                   cc::RenderFrameMetadata* out) {
  // Validate the scalar fields before touching `out`, so that a rejected
  // message never leaves the output struct partially populated with untrusted
  // values.
  const float device_scale_factor = data.device_scale_factor();
  if (!std::isfinite(device_scale_factor) || device_scale_factor <= 0.f) {
    SetFailedCheckCrashKey("device_scale_factor");
    return false;
  }
  const float page_scale_factor = data.page_scale_factor();
  if (!std::isfinite(page_scale_factor) || page_scale_factor <= 0.f) {
    SetFailedCheckCrashKey("page_scale_factor");
    return false;
  }
  const float external_page_scale_factor = data.external_page_scale_factor();
  if (!std::isfinite(external_page_scale_factor) ||
      external_page_scale_factor <= 0.f) {
    SetFailedCheckCrashKey("external_page_scale_factor");
    return false;
  }
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const float min_page_scale_factor = data.min_page_scale_factor();
  if (!std::isfinite(min_page_scale_factor) || min_page_scale_factor < 0.f) {
    SetFailedCheckCrashKey("min_page_scale_factor");
    return false;
  }
  const float max_page_scale_factor = data.max_page_scale_factor();
  if (!std::isfinite(max_page_scale_factor) || max_page_scale_factor < 0.f ||
      max_page_scale_factor < min_page_scale_factor) {
    SetFailedCheckCrashKey("max_page_scale_factor");
    return false;
  }
#endif

  out->is_scroll_offset_at_top = data.is_scroll_offset_at_top();
  out->is_mobile_optimized = data.is_mobile_optimized();
  out->device_scale_factor = device_scale_factor;
  out->page_scale_factor = page_scale_factor;
  out->external_page_scale_factor = external_page_scale_factor;
  out->primary_main_frame_item_sequence_number =
      data.primary_main_frame_item_sequence_number();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  out->min_page_scale_factor = min_page_scale_factor;
  out->max_page_scale_factor = max_page_scale_factor;
  out->root_overflow_y_hidden = data.root_overflow_y_hidden();
  out->has_transparent_background = data.has_transparent_background();
#endif

  if (!data.ReadBrowserControlsMetadata(&out->browser_controls_metadata)) {
    SetFailedCheckCrashKey("browser_controls_metadata");
    return false;
  }
  if (!data.ReadRootScrollOffset(&out->root_scroll_offset)) {
    SetFailedCheckCrashKey("root_scroll_offset");
    return false;
  }
  if (!data.ReadSelection(&out->selection)) {
    SetFailedCheckCrashKey("selection");
    return false;
  }
  if (!data.ReadDelegatedInkMetadata(&out->delegated_ink_metadata)) {
    SetFailedCheckCrashKey("delegated_ink_metadata");
    return false;
  }
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  if (!data.ReadScrollableViewportSize(&out->scrollable_viewport_size)) {
    SetFailedCheckCrashKey("scrollable_viewport_size");
    return false;
  }
  if (!data.ReadRootLayerSize(&out->root_layer_size)) {
    SetFailedCheckCrashKey("root_layer_size");
    return false;
  }
#endif
  if (!data.ReadTrackedElementRects(&out->tracked_element_rects)) {
    SetFailedCheckCrashKey("tracked_element_rects");
    return false;
  }
  if (!data.ReadViewportSizeInPixels(&out->viewport_size_in_pixels)) {
    SetFailedCheckCrashKey("viewport_size_in_pixels");
    return false;
  }
  if (!data.ReadLocalSurfaceId(&out->local_surface_id)) {
    SetFailedCheckCrashKey("local_surface_id");
    return false;
  }
  if (!data.ReadNewVerticalScrollDirection(
          &out->new_vertical_scroll_direction)) {
    SetFailedCheckCrashKey("new_vertical_scroll_direction");
    return false;
  }
  if (!data.ReadRootBackgroundColor(&out->root_background_color)) {
    SetFailedCheckCrashKey("root_background_color");
    return false;
  }
  return true;
}

}  // namespace mojo
