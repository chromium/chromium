// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_scrollbar_layer_impl.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "cc/input/scrollbar.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

std::unique_ptr<PaintedScrollbarLayerImpl> PaintedScrollbarLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation,
    bool is_left_side_vertical_scrollbar,
    bool is_overlay) {
  return base::WrapUnique(new PaintedScrollbarLayerImpl(
      tree_impl, id, orientation, is_left_side_vertical_scrollbar, is_overlay));
}

PaintedScrollbarLayerImpl::PaintedScrollbarLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation,
    bool is_left_side_vertical_scrollbar,
    bool is_overlay)
    : ScrollbarLayerImplBase(tree_impl,
                             id,
                             orientation,
                             is_left_side_vertical_scrollbar,
                             is_overlay) {}

PaintedScrollbarLayerImpl::~PaintedScrollbarLayerImpl() = default;

mojom::LayerType PaintedScrollbarLayerImpl::GetLayerType() const {
  return mojom::LayerType::kPaintedScrollbar;
}

std::unique_ptr<LayerImpl> PaintedScrollbarLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return PaintedScrollbarLayerImpl::Create(tree_impl, id(), orientation(),
                                           is_left_side_vertical_scrollbar(),
                                           is_overlay_scrollbar());
}

void PaintedScrollbarLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  ScrollbarLayerImplBase::PushPropertiesTo(layer);

  PaintedScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedScrollbarLayerImpl*>(layer);

  scrollbar_layer->set_internal_contents_scale_and_bounds(
      internal_contents_scale_, internal_content_bounds_);

  scrollbar_layer->SetJumpOnTrackClick(jump_on_track_click_);
  scrollbar_layer->SetSupportsDragSnapBack(supports_drag_snap_back_);
  scrollbar_layer->SetThumbThickness(thumb_thickness_);
  scrollbar_layer->SetThumbLength(thumb_length_);
  scrollbar_layer->SetBackButtonRect(back_button_rect_);
  scrollbar_layer->SetForwardButtonRect(forward_button_rect_);
  scrollbar_layer->SetTrackRect(track_rect_);

  scrollbar_layer->set_track_and_buttons_ui_resource_id(
      track_and_buttons_ui_resource_id_);
  scrollbar_layer->set_thumb_ui_resource_id(thumb_ui_resource_id_);
  scrollbar_layer->set_uses_nine_patch_track_and_buttons(
      uses_nine_patch_track_and_buttons_);

  scrollbar_layer->SetScrollbarPaintedOpacity(painted_opacity_);
  if (thumb_color_.has_value()) {
    scrollbar_layer->SetThumbColor(thumb_color_.value());
  }
  scrollbar_layer->SetTrackAndButtonsImageBounds(
      track_and_buttons_image_bounds_);
  scrollbar_layer->SetTrackAndButtonsAperture(track_and_buttons_aperture_);
}

float PaintedScrollbarLayerImpl::OverlayScrollbarOpacity() const {
  return IsFluentOverlayScrollbarEnabled() ? Opacity() : painted_opacity_;
}

bool PaintedScrollbarLayerImpl::WillDraw(
    DrawMode draw_mode,
    viz::ClientResourceProvider* resource_provider) {
  DCHECK(draw_mode != DRAW_MODE_RESOURCELESS_SOFTWARE);
  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void PaintedScrollbarLayerImpl::AppendQuads(
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  AppendThumbQuads(render_pass, append_quads_data);
  AppendTrackAndButtonsQuads(render_pass, append_quads_data);
}

void PaintedScrollbarLayerImpl::AppendThumbQuads(
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data) const {
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  if (thumb_color_.has_value()) {
    const gfx::Rect thumb_rect = ComputeThumbQuadRect();
    if (thumb_rect.IsEmpty()) {
      return;
    }
    gfx::Rect visible_thumb_rect =
        draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
            thumb_rect);
    visible_thumb_rect.Intersect(visible_layer_rect());
    if (visible_thumb_rect.IsEmpty()) {
      return;
    }

    gfx::MaskFilterInfo rounded_corners_mask =
        draw_properties().mask_filter_info;
    // Web tests draw the thumb as a square to avoid issues that come with the
    // differences in calculation of anti-aliasing and rounding in different
    // platforms.
    if (!is_web_test() && IsFluentScrollbarEnabled()) {
      const int rounded_corner_radius =
          orientation() == ScrollbarOrientation::kHorizontal
              ? thumb_rect.height()
              : thumb_rect.width();
      rounded_corners_mask = gfx::MaskFilterInfo(
          gfx::RRectF(gfx::RectF(thumb_rect), rounded_corner_radius));
      rounded_corners_mask.ApplyTransform(
          draw_properties().target_space_transform);
    }
    shared_quad_state->SetAll(
        draw_properties().target_space_transform, thumb_rect,
        visible_thumb_rect, rounded_corners_mask, /*clip=*/std::nullopt,
        /*contents_opaque=*/false, draw_properties().opacity,
        /*blend=*/SkBlendMode::kSrcOver, GetSortingContextId(),
        static_cast<uint32_t>(id()),
        /*fast_rounded_corner=*/true);
    auto* thumb_quad =
        render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    thumb_quad->SetNew(shared_quad_state, thumb_rect, visible_thumb_rect,
                       thumb_color_.value(), /*anti_aliasing_off=*/false);
    ValidateQuadResources(thumb_quad);
    return;
  }

  // If Fluent scrollbars are enabled but there is no `thumb_color_`, that means
  // that the scrollbar's bounds or thumb have no dimensions so we can exit
  // early.
  if (IsFluentScrollbarEnabled()) {
    return;
  }

  // The thumb sqs must be non-opaque so that the track and buttons will not be
  // occluded in viz by the thumb's 'quad_layer_rect'.
  constexpr bool kContentsOpaque = false;
  PopulateScaledSharedQuadState(shared_quad_state, internal_contents_scale_,
                                kContentsOpaque);

  AppendDebugBorderQuad(render_pass, gfx::Rect(internal_content_bounds_),
                        shared_quad_state, append_quads_data);
  gfx::Rect thumb_quad_rect = ComputeThumbQuadRect();
  gfx::Rect scaled_thumb_quad_rect =
      gfx::ScaleToEnclosingRect(thumb_quad_rect, internal_contents_scale_);
  gfx::Rect visible_thumb_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          thumb_quad_rect);
  gfx::Rect scaled_visible_thumb_quad_rect = gfx::ScaleToEnclosingRect(
      visible_thumb_quad_rect, internal_contents_scale_);
  viz::ResourceId thumb_resource_id =
      layer_tree_impl()->ResourceIdForUIResource(thumb_ui_resource_id_);

  if (!thumb_resource_id || visible_thumb_quad_rect.IsEmpty()) {
    return;
  }

  shared_quad_state->opacity *= painted_opacity_;
  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  quad->SetNew(shared_quad_state, scaled_thumb_quad_rect,
               scaled_visible_thumb_quad_rect, /*needs_blending=*/true,
               thumb_resource_id, /*premultiplied=*/true,
               /*top_left=*/gfx::PointF(0.f, 0.f),
               /*bottom_right=*/gfx::PointF(1.f, 1.f),
               /*background=*/SkColors::kTransparent,
               /*flipped=*/false,
               /*nearest=*/false, /*secure_output=*/false,
               /*video_type=*/gfx::ProtectedVideoType::kClear);
  ValidateQuadResources(quad);
}

void PaintedScrollbarLayerImpl::AppendTrackAndButtonsQuads(
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  if (IsFluentOverlayScrollbarEnabled() &&
      thumb_thickness_scale_factor() <= GetIdleThicknessScale() &&
      !has_find_in_page_tickmarks()) {
    return;
  }

  gfx::Rect track_and_buttons_quad_rect(bounds());
  gfx::Rect visible_track_and_buttons_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          track_and_buttons_quad_rect);
  viz::ResourceId track_and_buttons_resource_id =
      layer_tree_impl()->ResourceIdForUIResource(
          track_and_buttons_ui_resource_id_);

  if (!track_and_buttons_resource_id || track_and_buttons_quad_rect.IsEmpty()) {
    return;
  }

  viz::SharedQuadState* track_and_buttons_shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateScaledSharedQuadState(track_and_buttons_shared_quad_state,
                                internal_contents_scale_, contents_opaque());
  if (IsFluentOverlayScrollbarEnabled()) {
    // Scale the opacity value linearly in function of the current thumb
    // thickness. When thickness scale factor is kIdleThickness, then the
    // track's opacity should be zero. When the thickness scale factor
    // reaches its maximum value (1.f), then the opacity of the tracks should
    // reach it's maximum value (1.f).
    CHECK_GE(thumb_thickness_scale_factor(), GetIdleThicknessScale());
    CHECK_LE(thumb_thickness_scale_factor(), 1.f);
    const float scaled_opacity =
        (thumb_thickness_scale_factor() - GetIdleThicknessScale()) /
        (1.f - GetIdleThicknessScale());
    track_and_buttons_shared_quad_state->opacity *= scaled_opacity;
  }

  if (uses_nine_patch_track_and_buttons_ && !has_find_in_page_tickmarks()) {
    AppendNinePatchScaledTrackAndButtons(render_pass,
                                         track_and_buttons_shared_quad_state,
                                         track_and_buttons_quad_rect);
    return;
  }

  gfx::Rect scaled_track_and_buttons_quad_rect(internal_content_bounds_);
  gfx::Rect scaled_visible_track_and_buttons_quad_rect =
      gfx::ScaleToEnclosingRect(visible_track_and_buttons_quad_rect,
                                internal_contents_scale_);
  bool needs_blending = !contents_opaque();
  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  quad->SetNew(track_and_buttons_shared_quad_state,
               scaled_track_and_buttons_quad_rect,
               scaled_visible_track_and_buttons_quad_rect, needs_blending,
               track_and_buttons_resource_id, /*premultiplied=*/true,
               /*top_left=*/gfx::PointF(0.f, 0.f),
               /*bottom_right=*/gfx::PointF(1.f, 1.f),
               /*background=*/SkColors::kTransparent,
               /*flipped=*/false,
               /*nearest=*/false, /*secure_output=*/false,
               /*video_type=*/gfx::ProtectedVideoType::kClear);
  ValidateQuadResources(quad);
}

void PaintedScrollbarLayerImpl::AppendNinePatchScaledTrackAndButtons(
    viz::CompositorRenderPass* render_pass,
    viz::SharedQuadState* shared_quad_state,
    gfx::Rect& track_and_buttons_quad_rect) {
  CHECK(uses_nine_patch_track_and_buttons_);
  gfx::Rect border(
      track_and_buttons_aperture_.x(), track_and_buttons_aperture_.y(),
      track_and_buttons_aperture_.x() * 2, track_and_buttons_aperture_.y() * 2);
  gfx::Rect layer_occlusion;
  bool layout_changed = track_and_buttons_patch_generator_.SetLayout(
      track_and_buttons_image_bounds_, track_and_buttons_quad_rect.size(),
      track_and_buttons_aperture_, border, layer_occlusion,
      /*fill_center=*/true, /*nearest_neighbor=*/false);
  if (layout_changed) {
    track_and_buttons_patch_generator_.CheckGeometryLimitations();
    track_and_buttons_patches_ =
        track_and_buttons_patch_generator_.GeneratePatches();
    gfx::Vector2d offset = track_and_buttons_quad_rect.OffsetFromOrigin();
    for (auto& patch : track_and_buttons_patches_) {
      patch.output_rect += offset;
    }
  }

  track_and_buttons_patch_generator_.AppendQuadsForCc(
      this, track_and_buttons_ui_resource_id_, render_pass, shared_quad_state,
      track_and_buttons_patches_);
}

gfx::Rect PaintedScrollbarLayerImpl::GetEnclosingVisibleRectInTargetSpace()
    const {
  if (internal_content_bounds_.IsEmpty())
    return gfx::Rect();
  DCHECK_GT(internal_contents_scale_, 0.f);
  return GetScaledEnclosingVisibleRectInTargetSpace(internal_contents_scale_);
}

gfx::Rect PaintedScrollbarLayerImpl::ComputeThumbQuadRect() const {
  gfx::Rect thumb_rect = ScrollbarLayerImplBase::ComputeThumbQuadRect();
  if (thumb_color_.has_value()) {
    thumb_rect = CenterSolidColorThumb(thumb_rect);
  }
  return thumb_rect;
}

gfx::Rect PaintedScrollbarLayerImpl::ComputeHitTestableThumbQuadRect() const {
  if (thumb_color_.has_value()) {
    return ExpandSolidColorThumb(ComputeThumbQuadRect());
  }
  return ScrollbarLayerImplBase::ComputeHitTestableThumbQuadRect();
}

gfx::Rect PaintedScrollbarLayerImpl::ComputeHitTestableExpandedThumbQuadRect()
    const {
  CHECK(is_overlay_scrollbar());
  gfx::Rect thumb_rect =
      ScrollbarLayerImplBase::ComputeHitTestableExpandedThumbQuadRect();
  if (thumb_color_.has_value()) {
    thumb_rect = ExpandSolidColorThumb(CenterSolidColorThumb(thumb_rect));
  }
  return thumb_rect;
}

gfx::Rect PaintedScrollbarLayerImpl::CenterSolidColorThumb(
    gfx::Rect thumb_rect) const {
  CHECK(thumb_color_.has_value());
  const int track_thickness = orientation() == ScrollbarOrientation::kHorizontal
                                  ? track_rect_.height()
                                  : track_rect_.width();
  const int thumb_offset =
      static_cast<int>((track_thickness - ThumbThickness()) / 2.0f);

  if (orientation() == ScrollbarOrientation::kHorizontal) {
    thumb_rect.Offset(0, thumb_offset);
  } else {
    thumb_rect.Offset(
        is_left_side_vertical_scrollbar() ? -thumb_offset : thumb_offset, 0);
  }
  return thumb_rect;
}

gfx::Rect PaintedScrollbarLayerImpl::ExpandSolidColorThumb(
    gfx::Rect thumb_rect) const {
  CHECK(thumb_color_.has_value());
  const gfx::Rect back_track_rect = BackTrackRect();
  if (orientation() == ScrollbarOrientation::kHorizontal) {
    thumb_rect.set_y(back_track_rect.y());
    thumb_rect.set_height(back_track_rect.height());
    return thumb_rect;
  }
  thumb_rect.set_x(back_track_rect.x());
  thumb_rect.set_width(back_track_rect.width());
  return thumb_rect;
}

void PaintedScrollbarLayerImpl::SetJumpOnTrackClick(bool jump_on_track_click) {
  if (jump_on_track_click_ == jump_on_track_click)
    return;
  jump_on_track_click_ = jump_on_track_click;
  NoteLayerPropertyChanged();
}

bool PaintedScrollbarLayerImpl::JumpOnTrackClick() const {
  return jump_on_track_click_;
}

void PaintedScrollbarLayerImpl::SetSupportsDragSnapBack(
    bool supports_drag_snap_back) {
  if (supports_drag_snap_back_ == supports_drag_snap_back)
    return;
  supports_drag_snap_back_ = supports_drag_snap_back;
  NoteLayerPropertyChanged();
}

void PaintedScrollbarLayerImpl::SetTrackAndButtonsImageBounds(
    const gfx::Size& bounds) {
  if (track_and_buttons_image_bounds_ == bounds) {
    return;
  }
  track_and_buttons_image_bounds_ = bounds;
  NoteLayerPropertyChanged();
}

void PaintedScrollbarLayerImpl::SetTrackAndButtonsAperture(
    const gfx::Rect& aperture) {
  if (track_and_buttons_aperture_ == aperture) {
    return;
  }
  track_and_buttons_aperture_ = aperture;
  NoteLayerPropertyChanged();
}

void PaintedScrollbarLayerImpl::SetThumbColor(SkColor4f thumb_color) {
  if (thumb_color_ == thumb_color) {
    return;
  }
  thumb_color_ = thumb_color;
  NoteLayerPropertyChanged();
}

bool PaintedScrollbarLayerImpl::SupportsDragSnapBack() const {
  return supports_drag_snap_back_;
}

void PaintedScrollbarLayerImpl::SetThumbThickness(int thumb_thickness) {
  if (thumb_thickness_ == thumb_thickness)
    return;
  thumb_thickness_ = thumb_thickness;
  NoteLayerPropertyChanged();
}

int PaintedScrollbarLayerImpl::ThumbThickness() const {
  return thumb_thickness_;
}

void PaintedScrollbarLayerImpl::SetThumbLength(int thumb_length) {
  if (thumb_length_ == thumb_length)
    return;
  thumb_length_ = thumb_length;
  NoteLayerPropertyChanged();
}

int PaintedScrollbarLayerImpl::ThumbLength() const {
  return thumb_length_;
}

int PaintedScrollbarLayerImpl::TrackStart() const {
  return orientation() == ScrollbarOrientation::kVertical ? track_rect_.y()
                                                          : track_rect_.x();
}

void PaintedScrollbarLayerImpl::SetBackButtonRect(gfx::Rect back_button_rect) {
  if (back_button_rect_ == back_button_rect)
    return;
  back_button_rect_ = back_button_rect;
  NoteLayerPropertyChanged();
}

gfx::Rect PaintedScrollbarLayerImpl::BackButtonRect() const {
  return back_button_rect_;
}

void PaintedScrollbarLayerImpl::SetForwardButtonRect(
    gfx::Rect forward_button_rect) {
  if (forward_button_rect_ == forward_button_rect)
    return;
  forward_button_rect_ = forward_button_rect;
  NoteLayerPropertyChanged();
}

gfx::Rect PaintedScrollbarLayerImpl::ForwardButtonRect() const {
  return forward_button_rect_;
}

gfx::Rect PaintedScrollbarLayerImpl::BackTrackRect() const {
  const gfx::Rect thumb_rect = ComputeThumbQuadRect();
  const int rect_x = track_rect_.x();
  const int rect_y = track_rect_.y();
  if (orientation() == ScrollbarOrientation::kHorizontal) {
    int width = thumb_rect.x() - rect_x;
    int height = track_rect_.height();
    return gfx::Rect(rect_x, rect_y, width, height);
  } else {
    int width = track_rect_.width();
    int height = thumb_rect.y() - rect_y;
    return gfx::Rect(rect_x, rect_y, width, height);
  }
}

gfx::Rect PaintedScrollbarLayerImpl::ForwardTrackRect() const {
  const gfx::Rect thumb_rect = ComputeThumbQuadRect();
  const int track_end = TrackStart() + TrackLength();
  if (orientation() == ScrollbarOrientation::kHorizontal) {
    int rect_x = thumb_rect.right();
    int rect_y = track_rect_.y();
    int width = track_end - rect_x;
    int height = track_rect_.height();
    return gfx::Rect(rect_x, rect_y, width, height);
  } else {
    int rect_x = track_rect_.x();
    int rect_y = thumb_rect.bottom();
    int width = track_rect_.width();
    int height = track_end - rect_y;
    return gfx::Rect(rect_x, rect_y, width, height);
  }
}

void PaintedScrollbarLayerImpl::SetTrackRect(gfx::Rect track_rect) {
  if (track_rect_ == track_rect)
    return;
  track_rect_ = track_rect;
  NoteLayerPropertyChanged();
}

void PaintedScrollbarLayerImpl::SetScrollbarPaintedOpacity(float opacity) {
  if (painted_opacity_ == opacity)
    return;
  painted_opacity_ = opacity;
  NoteLayerPropertyChanged();
}

float PaintedScrollbarLayerImpl::TrackLength() const {
  if (orientation() == ScrollbarOrientation::kVertical) {
    return track_rect_.height() + vertical_adjust();
  } else {
    return track_rect_.width();
  }
}

bool PaintedScrollbarLayerImpl::IsThumbResizable() const {
  return false;
}

LayerTreeSettings::ScrollbarAnimator
PaintedScrollbarLayerImpl::GetScrollbarAnimator() const {
  return IsFluentOverlayScrollbarEnabled() ? LayerTreeSettings::AURA_OVERLAY
                                           : LayerTreeSettings::NO_ANIMATOR;
}

}  // namespace cc
