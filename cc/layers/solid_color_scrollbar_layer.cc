// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_scrollbar_layer.h"

#include <memory>

#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"

namespace cc {

std::unique_ptr<LayerImpl> SolidColorScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  const bool kIsOverlayScrollbar = true;
  return SolidColorScrollbarLayerImpl::Create(
      tree_impl, id(), solid_color_scrollbar_layer_inputs_.orientation,
      solid_color_scrollbar_layer_inputs_.thumb_thickness,
      solid_color_scrollbar_layer_inputs_.track_start,
      solid_color_scrollbar_layer_inputs_.is_left_side_vertical_scrollbar,
      kIsOverlayScrollbar);
}

scoped_refptr<SolidColorScrollbarLayer> SolidColorScrollbarLayer::Create(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar,
    ElementId scroll_element_id) {
  return base::WrapRefCounted(new SolidColorScrollbarLayer(
      orientation, thumb_thickness, track_start,
      is_left_side_vertical_scrollbar, scroll_element_id));
}

SolidColorScrollbarLayer::SolidColorScrollbarLayerInputs::
    SolidColorScrollbarLayerInputs(ScrollbarOrientation orientation,
                                   int thumb_thickness,
                                   int track_start,
                                   bool is_left_side_vertical_scrollbar,
                                   ElementId scroll_element_id)
    : scroll_element_id(scroll_element_id),
      orientation(orientation),
      thumb_thickness(thumb_thickness),
      track_start(track_start),
      is_left_side_vertical_scrollbar(is_left_side_vertical_scrollbar) {}

SolidColorScrollbarLayer::SolidColorScrollbarLayerInputs::
    ~SolidColorScrollbarLayerInputs() = default;

SolidColorScrollbarLayer::SolidColorScrollbarLayer(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar,
    ElementId scroll_element_id)
    : solid_color_scrollbar_layer_inputs_(orientation,
                                          thumb_thickness,
                                          track_start,
                                          is_left_side_vertical_scrollbar,
                                          scroll_element_id) {
  Layer::SetOpacity(0.f);
  SetIsScrollbar(true);
}

SolidColorScrollbarLayer::~SolidColorScrollbarLayer() = default;

void SolidColorScrollbarLayer::SetOpacity(float opacity) {
  // The opacity of a solid color scrollbar layer is always 0 on main thread.
  DCHECK_EQ(opacity, 0.f);
  Layer::SetOpacity(opacity);
}

void SolidColorScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  SolidColorScrollbarLayerImpl* scrollbar_layer =
      static_cast<SolidColorScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetScrollElementId(
      solid_color_scrollbar_layer_inputs_.scroll_element_id);
}

void SolidColorScrollbarLayer::SetNeedsDisplayRect(const gfx::Rect& rect) {
  // Never needs repaint.
}

bool SolidColorScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return true;
}

void SolidColorScrollbarLayer::SetScrollElementId(ElementId element_id) {
  if (element_id == solid_color_scrollbar_layer_inputs_.scroll_element_id)
    return;

  solid_color_scrollbar_layer_inputs_.scroll_element_id = element_id;
  SetNeedsCommit();
}

}  // namespace cc
