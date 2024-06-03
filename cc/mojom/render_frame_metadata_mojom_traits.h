// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_RENDER_FRAME_METADATA_MOJOM_TRAITS_H_
#define CC_MOJOM_RENDER_FRAME_METADATA_MOJOM_TRAITS_H_

#include <optional>

#include "base/component_export.h"
#include "build/build_config.h"
#include "cc/mojom/render_frame_metadata.mojom-shared.h"
#include "cc/trees/render_frame_metadata.h"
#include "services/viz/public/cpp/compositing/local_surface_id_mojom_traits.h"
#include "skia/public/mojom/skcolor4f_mojom_traits.h"
#include "third_party/skia/include/core/SkColor.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(CC_SHARED_MOJOM_TRAITS)
    StructTraits<cc::mojom::DelegatedInkBrowserMetadataDataView,
                 cc::DelegatedInkBrowserMetadata> {
  static bool delegated_ink_is_hovering(
      const cc::DelegatedInkBrowserMetadata& metadata) {
    return metadata.delegated_ink_is_hovering;
  }

  static bool Read(cc::mojom::DelegatedInkBrowserMetadataDataView data,
                   cc::DelegatedInkBrowserMetadata* out);
};

template <>
struct COMPONENT_EXPORT(CC_SHARED_MOJOM_TRAITS)
    StructTraits<cc::mojom::RenderFrameMetadataDataView,
                 cc::RenderFrameMetadata> {
  static SkColor4f root_background_color(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.root_background_color;
  }

  static const std::optional<gfx::PointF>& root_scroll_offset(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.root_scroll_offset;
  }

  static bool is_scroll_offset_at_top(const cc::RenderFrameMetadata& metadata) {
    return metadata.is_scroll_offset_at_top;
  }

  static const viz::Selection<gfx::SelectionBound>& selection(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.selection;
  }

  static bool is_mobile_optimized(const cc::RenderFrameMetadata& metadata) {
    return metadata.is_mobile_optimized;
  }

  static const std::optional<cc::DelegatedInkBrowserMetadata>&
  delegated_ink_metadata(const cc::RenderFrameMetadata& metadata) {
    return metadata.delegated_ink_metadata;
  }

  static float device_scale_factor(const cc::RenderFrameMetadata& metadata) {
    return metadata.device_scale_factor;
  }

  static const gfx::Size& viewport_size_in_pixels(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.viewport_size_in_pixels;
  }

  static const std::optional<viz::LocalSurfaceId>& local_surface_id(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.local_surface_id;
  }

  static float page_scale_factor(const cc::RenderFrameMetadata& metadata) {
    return metadata.page_scale_factor;
  }

  static float external_page_scale_factor(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.external_page_scale_factor;
  }

  static float top_controls_height(const cc::RenderFrameMetadata& metadata) {
    return metadata.top_controls_height;
  }

  static float top_controls_shown_ratio(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.top_controls_shown_ratio;
  }

  static viz::VerticalScrollDirection new_vertical_scroll_direction(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.new_vertical_scroll_direction;
  }

  static int64_t primary_main_frame_item_sequence_number(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.primary_main_frame_item_sequence_number;
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  static float bottom_controls_height(const cc::RenderFrameMetadata& metadata) {
    return metadata.bottom_controls_height;
  }

  static float bottom_controls_shown_ratio(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.bottom_controls_shown_ratio;
  }

  static float top_controls_min_height_offset(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.top_controls_min_height_offset;
  }

  static float bottom_controls_min_height_offset(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.bottom_controls_min_height_offset;
  }

  static float min_page_scale_factor(const cc::RenderFrameMetadata& metadata) {
    return metadata.min_page_scale_factor;
  }

  static float max_page_scale_factor(const cc::RenderFrameMetadata& metadata) {
    return metadata.max_page_scale_factor;
  }

  static bool root_overflow_y_hidden(const cc::RenderFrameMetadata& metadata) {
    return metadata.root_overflow_y_hidden;
  }

  static const gfx::SizeF& scrollable_viewport_size(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.scrollable_viewport_size;
  }

  static const gfx::SizeF& root_layer_size(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.root_layer_size;
  }

  static bool has_transparent_background(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.has_transparent_background;
  }
#endif

  static bool Read(cc::mojom::RenderFrameMetadataDataView data,
                   cc::RenderFrameMetadata* out);
};

}  // namespace mojo

#endif  // CC_MOJOM_RENDER_FRAME_METADATA_MOJOM_TRAITS_H_
