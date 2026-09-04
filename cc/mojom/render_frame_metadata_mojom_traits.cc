// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojom/render_frame_metadata_mojom_traits.h"

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
      "rfm_failed_check", base::debug::CrashKeySize::Size32);
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
  out->top_controls_height = data.top_controls_height();
  out->top_controls_shown_ratio = data.top_controls_shown_ratio();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  out->bottom_controls_height = data.bottom_controls_height();
  out->bottom_controls_shown_ratio = data.bottom_controls_shown_ratio();
  out->top_controls_min_height_offset = data.top_controls_min_height_offset();
  out->bottom_controls_min_height_offset =
      data.bottom_controls_min_height_offset();
#endif
  return true;
}

// static
bool StructTraits<
    cc::mojom::RenderFrameMetadataDataView,
    cc::RenderFrameMetadata>::Read(cc::mojom::RenderFrameMetadataDataView data,
                                   cc::RenderFrameMetadata* out) {
  out->is_scroll_offset_at_top = data.is_scroll_offset_at_top();
  out->is_mobile_optimized = data.is_mobile_optimized();
  out->device_scale_factor = data.device_scale_factor();
  out->page_scale_factor = data.page_scale_factor();
  out->external_page_scale_factor = data.external_page_scale_factor();
  out->primary_main_frame_item_sequence_number =
      data.primary_main_frame_item_sequence_number();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  out->min_page_scale_factor = data.min_page_scale_factor();
  out->max_page_scale_factor = data.max_page_scale_factor();
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
