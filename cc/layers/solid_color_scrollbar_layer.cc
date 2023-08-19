// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_scrollbar_layer.h"

#include <memory>

#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"

namespace cc {

std::unique_ptr<LayerImpl> SolidColorScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return SolidColorScrollbarLayerImpl::Create(
      tree_impl, id(), orientation(), thumb_thickness_, track_start_,
      is_left_side_vertical_scrollbar());
}

scoped_refptr<SolidColorScrollbarLayer> SolidColorScrollbarLayer::CreateOrReuse(
    scoped_refptr<Scrollbar> scrollbar,
    SolidColorScrollbarLayer* existing_layer) {
  DCHECK(scrollbar->IsOverlay());
  bool is_horizontal =
      scrollbar->Orientation() == ScrollbarOrientation::HORIZONTAL;
  gfx::Rect thumb_rect = scrollbar->ThumbRect();
  int thumb_thickness =
      is_horizontal ? thumb_rect.height() : thumb_rect.width();
  gfx::Rect track_rect = scrollbar->TrackRect();
  int track_start = is_horizontal ? track_rect.x() : track_rect.y();

  if (existing_layer &&
      // We don't support change of these fields in a layer.
      existing_layer->thumb_thickness() == thumb_thickness &&
      existing_layer->track_start() == track_start) {
    // These fields have been checked in ScrollbarLayerBase::CreateOrReuse().
    DCHECK_EQ(scrollbar->Orientation(), existing_layer->orientation());
    DCHECK_EQ(scrollbar->IsLeftSideVerticalScrollbar(),
              existing_layer->is_left_side_vertical_scrollbar());
    return existing_layer;
  }

  return Create(scrollbar->Orientation(), thumb_thickness, track_start,
                scrollbar->IsLeftSideVerticalScrollbar());
}

scoped_refptr<SolidColorScrollbarLayer> SolidColorScrollbarLayer::Create(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar) {
  return base::WrapRefCounted(
      new SolidColorScrollbarLayer(orientation, thumb_thickness, track_start,
                                   is_left_side_vertical_scrollbar));
}

SolidColorScrollbarLayer::SolidColorScrollbarLayer(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar)
    : ScrollbarLayerBase(orientation, is_left_side_vertical_scrollbar),
      thumb_thickness_(thumb_thickness),
      track_start_(track_start) {
  Layer::SetOpacity(0.f);
}

SolidColorScrollbarLayer::~SolidColorScrollbarLayer() = default;

void SolidColorScrollbarLayer::SetOpacity(float opacity) {
  // The opacity of a solid color scrollbar layer is always 0 on main thread.
  DCHECK_EQ(opacity, 0.f);
  Layer::SetOpacity(opacity);
}

void SolidColorScrollbarLayer::SetNeedsDisplayRect(const gfx::Rect& rect) {}

bool SolidColorScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return true;
}

ScrollbarLayerBase::ScrollbarLayerType
SolidColorScrollbarLayer::GetScrollbarLayerType() const {
  return kSolidColor;
}

}  // namespace cc
