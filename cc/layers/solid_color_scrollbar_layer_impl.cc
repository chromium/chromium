// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_scrollbar_layer_impl.h"

#include "base/memory/ptr_util.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"

namespace cc {

std::unique_ptr<SolidColorScrollbarLayerImpl>
SolidColorScrollbarLayerImpl::Create(LayerTreeImpl* tree_impl,
                                     int id,
                                     ScrollbarOrientation orientation,
                                     int thumb_thickness,
                                     int track_start,
                                     bool is_left_side_vertical_scrollbar) {
  return base::WrapUnique(new SolidColorScrollbarLayerImpl(
      tree_impl, id, orientation, thumb_thickness, track_start,
      is_left_side_vertical_scrollbar));
}

SolidColorScrollbarLayerImpl::~SolidColorScrollbarLayerImpl() = default;

std::unique_ptr<LayerImpl> SolidColorScrollbarLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SolidColorScrollbarLayerImpl::Create(
      tree_impl, id(), orientation(), thumb_thickness_, track_start_,
      is_left_side_vertical_scrollbar());
}

SolidColorScrollbarLayerImpl::SolidColorScrollbarLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar)
    : ScrollbarLayerImplBase(tree_impl,
                             id,
                             orientation,
                             is_left_side_vertical_scrollbar,
                             /*is_overlay*/ true),
      thumb_thickness_(thumb_thickness),
      track_start_(track_start),
      color_(tree_impl->settings().solid_color_scrollbar_color) {}

void SolidColorScrollbarLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  ScrollbarLayerImplBase::PushPropertiesTo(layer);
  DCHECK(!layer->HitTestable());
}

int SolidColorScrollbarLayerImpl::ThumbThickness() const {
  if (thumb_thickness_ != -1)
    return thumb_thickness_;

  if (orientation() == HORIZONTAL)
    return bounds().height();
  else
    return bounds().width();
}

int SolidColorScrollbarLayerImpl::ThumbLength() const {
  float thumb_length = TrackLength();
  if (scroll_layer_length())
    thumb_length *= clip_layer_length() / scroll_layer_length();

  return std::max(static_cast<int>(thumb_length), ThumbThickness());
}

float SolidColorScrollbarLayerImpl::TrackLength() const {
  if (orientation() == HORIZONTAL)
    return bounds().width() - TrackStart() * 2;
  else
    return bounds().height() + vertical_adjust() - TrackStart() * 2;
}

int SolidColorScrollbarLayerImpl::TrackStart() const { return track_start_; }

bool SolidColorScrollbarLayerImpl::IsThumbResizable() const {
  return true;
}

void SolidColorScrollbarLayerImpl::AppendQuads(
    viz::RenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state, contents_opaque());

  AppendDebugBorderQuad(render_pass, gfx::Rect(bounds()), shared_quad_state,
                        append_quads_data);

  gfx::Rect thumb_quad_rect(ComputeThumbQuadRect());
  gfx::Rect visible_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          thumb_quad_rect);
  if (visible_quad_rect.IsEmpty())
    return;

  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(
      shared_quad_state, thumb_quad_rect, visible_quad_rect, color_, false);
}

const char* SolidColorScrollbarLayerImpl::LayerTypeAsString() const {
  return "cc::SolidColorScrollbarLayerImpl";
}

}  // namespace cc
