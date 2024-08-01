// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_thumb_scrollbar_layer_impl.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"

namespace cc {

std::unique_ptr<NinePatchThumbScrollbarLayerImpl>
NinePatchThumbScrollbarLayerImpl::Create(LayerTreeImpl* tree_impl,
                                         int id,
                                         ScrollbarOrientation orientation,
                                         bool is_left_side_vertical_scrollbar) {
  return base::WrapUnique(new NinePatchThumbScrollbarLayerImpl(
      tree_impl, id, orientation, is_left_side_vertical_scrollbar));
}

NinePatchThumbScrollbarLayerImpl::NinePatchThumbScrollbarLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation,
    bool is_left_side_vertical_scrollbar)
    : ScrollbarLayerImplBase(tree_impl,
                             id,
                             orientation,
                             is_left_side_vertical_scrollbar,
                             true) {}

NinePatchThumbScrollbarLayerImpl::~NinePatchThumbScrollbarLayerImpl() = default;

mojom::LayerType NinePatchThumbScrollbarLayerImpl::GetLayerType() const {
  return mojom::LayerType::kNinePatchThumbScrollbar;
}

std::unique_ptr<LayerImpl> NinePatchThumbScrollbarLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return NinePatchThumbScrollbarLayerImpl::Create(
      tree_impl, id(), orientation(), is_left_side_vertical_scrollbar());
}

void NinePatchThumbScrollbarLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  ScrollbarLayerImplBase::PushPropertiesTo(layer);

  NinePatchThumbScrollbarLayerImpl* scrollbar_layer =
      static_cast<NinePatchThumbScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetThumbThickness(thumb_thickness_);
  scrollbar_layer->SetThumbLength(thumb_length_);
  scrollbar_layer->SetTrackStart(track_start_);
  scrollbar_layer->SetTrackLength(track_length_);

  scrollbar_layer->SetImageBounds(image_bounds_);
  scrollbar_layer->SetAperture(aperture_);

  scrollbar_layer->set_thumb_ui_resource_id(thumb_ui_resource_id_);
  scrollbar_layer->set_track_and_buttons_ui_resource_id(
      track_and_buttons_ui_resource_id_);
}

bool NinePatchThumbScrollbarLayerImpl::WillDraw(
    DrawMode draw_mode,
    viz::ClientResourceProvider* resource_provider) {
  DCHECK(draw_mode != DRAW_MODE_RESOURCELESS_SOFTWARE);
  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void NinePatchThumbScrollbarLayerImpl::AppendQuads(
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  AppendThumbQuads(render_pass, append_quads_data, shared_quad_state);
  AppendTrackAndButtonsQuads(render_pass, append_quads_data, shared_quad_state);
}

void NinePatchThumbScrollbarLayerImpl::AppendThumbQuads(
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data,
    viz::SharedQuadState* shared_quad_state) {
  if (aperture_.IsEmpty())
    return;

  bool is_resource =
      thumb_ui_resource_id_ &&
      layer_tree_impl()->ResourceIdForUIResource(thumb_ui_resource_id_);
  bool are_contents_opaque =
      is_resource
          ? layer_tree_impl()->IsUIResourceOpaque(thumb_ui_resource_id_) ||
                contents_opaque()
          : false;
  PopulateSharedQuadState(shared_quad_state, are_contents_opaque);
  AppendDebugBorderQuad(render_pass, gfx::Rect(bounds()), shared_quad_state,
                        append_quads_data);

  if (!is_resource)
    return;

  // For overlay scrollbars, the border should match the inset of the aperture
  // and be symmetrical.
  gfx::Rect border(aperture_.x(), aperture_.y(), aperture_.x() * 2,
                   aperture_.y() * 2);
  gfx::Rect thumb_quad_rect(ComputeThumbQuadRect());
  gfx::Rect layer_occlusion;
  bool fill_center = true;
  bool nearest_neighbor = false;

  // Avoid drawing a scrollber in the degenerate case where the scroller is
  // smaller than the border size.
  if (thumb_quad_rect.height() < border.height() ||
      thumb_quad_rect.width() < border.width())
    return;

  const bool layout_changed = quad_generator_.SetLayout(
      image_bounds_, thumb_quad_rect.size(), aperture_, border, layer_occlusion,
      fill_center, nearest_neighbor);
  if (layout_changed) {
    quad_generator_.CheckGeometryLimitations();
    patches_ = quad_generator_.GeneratePatches();
  }

  quad_generator_.AppendQuadsForCc(this, thumb_ui_resource_id_, render_pass,
                                   shared_quad_state, patches_,
                                   thumb_quad_rect.OffsetFromOrigin());
}

void NinePatchThumbScrollbarLayerImpl::AppendTrackAndButtonsQuads(
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data,
    viz::SharedQuadState* shared_quad_state) {
  viz::ResourceId track_resource_id =
      layer_tree_impl()->ResourceIdForUIResource(
          track_and_buttons_ui_resource_id_);
  if (!track_resource_id)
    return;

  bool nearest_neighbor = false;

  gfx::Rect track_quad_rect(bounds());
  gfx::Rect scaled_track_quad_rect(bounds());
  gfx::Rect visible_track_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          track_quad_rect);
  gfx::Rect scaled_visible_track_quad_rect =
      gfx::ScaleToEnclosingRect(visible_track_quad_rect, 1.f);

  bool needs_blending = !contents_opaque();
  bool premultipled_alpha = true;
  bool flipped = false;
  gfx::PointF uv_top_left(0.f, 0.f);
  gfx::PointF uv_bottom_right(1.f, 1.f);
  viz::TextureDrawQuad* quad =
      render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  quad->SetNew(
      shared_quad_state, scaled_track_quad_rect, scaled_visible_track_quad_rect,
      needs_blending, track_resource_id, premultipled_alpha, uv_top_left,
      uv_bottom_right, SkColors::kTransparent, flipped, nearest_neighbor,
      /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);
  ValidateQuadResources(quad);
}

void NinePatchThumbScrollbarLayerImpl::SetThumbThickness(int thumb_thickness) {
  if (thumb_thickness_ == thumb_thickness)
    return;
  thumb_thickness_ = thumb_thickness;
  NoteLayerPropertyChanged();
}

int NinePatchThumbScrollbarLayerImpl::ThumbThickness() const {
  return thumb_thickness_;
}

void NinePatchThumbScrollbarLayerImpl::SetThumbLength(int thumb_length) {
  if (thumb_length_ == thumb_length)
    return;
  thumb_length_ = thumb_length;
  NoteLayerPropertyChanged();
}

int NinePatchThumbScrollbarLayerImpl::ThumbLength() const {
  return thumb_length_;
}

void NinePatchThumbScrollbarLayerImpl::SetTrackStart(int track_start) {
  if (track_start_ == track_start)
    return;
  track_start_ = track_start;
  NoteLayerPropertyChanged();
}

int NinePatchThumbScrollbarLayerImpl::TrackStart() const {
  return track_start_;
}

void NinePatchThumbScrollbarLayerImpl::SetTrackLength(int track_length) {
  if (track_length_ == track_length)
    return;
  track_length_ = track_length;
  NoteLayerPropertyChanged();
}

void NinePatchThumbScrollbarLayerImpl::SetImageBounds(const gfx::Size& bounds) {
  if (image_bounds_ == bounds)
    return;
  image_bounds_ = bounds;
  NoteLayerPropertyChanged();
}

void NinePatchThumbScrollbarLayerImpl::SetAperture(const gfx::Rect& aperture) {
  if (aperture_ == aperture)
    return;
  aperture_ = aperture;
  NoteLayerPropertyChanged();
}

float NinePatchThumbScrollbarLayerImpl::TrackLength() const {
  return track_length_ + (orientation() == ScrollbarOrientation::kVertical
                              ? vertical_adjust()
                              : 0);
}

bool NinePatchThumbScrollbarLayerImpl::IsThumbResizable() const {
  return false;
}

}  // namespace cc
