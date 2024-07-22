// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_scrollbar_layer.h"

#include <memory>

#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"

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
      scrollbar->Orientation() == ScrollbarOrientation::kHorizontal;
  gfx::Rect thumb_rect = scrollbar->ThumbRect();
  int thumb_thickness =
      is_horizontal ? thumb_rect.height() : thumb_rect.width();
  gfx::Rect track_rect = scrollbar->TrackRect();
  int track_start = is_horizontal ? track_rect.x() : track_rect.y();

  scoped_refptr<SolidColorScrollbarLayer> result;
  if (existing_layer &&
      // We don't support change of these fields in a layer.
      existing_layer->thumb_thickness() == thumb_thickness &&
      existing_layer->track_start() == track_start) {
    // These fields have been checked in ScrollbarLayerBase::CreateOrReuse().
    DCHECK_EQ(scrollbar->Orientation(), existing_layer->orientation());
    DCHECK_EQ(scrollbar->IsLeftSideVerticalScrollbar(),
              existing_layer->is_left_side_vertical_scrollbar());
    result = existing_layer;
  } else {
    result = Create(scrollbar->Orientation(), thumb_thickness, track_start,
                    scrollbar->IsLeftSideVerticalScrollbar());
  }
  result->SetColor(scrollbar->ThumbColor());
  return result;
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
      track_start_(track_start),
      color_(SkColors::kTransparent) {
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

void SolidColorScrollbarLayer::PushPropertiesTo(
    LayerImpl* layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  ScrollbarLayerBase::PushPropertiesTo(layer, commit_state, unsafe_state);
  static_cast<SolidColorScrollbarLayerImpl*>(layer)->set_color(color());
}

void SolidColorScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (host != layer_tree_host()) {
    ScrollbarLayerBase::SetLayerTreeHost(host);
    SetColor(color());
  }
}

void SolidColorScrollbarLayer::SetColor(SkColor4f color) {
  if (layer_tree_host() &&
      layer_tree_host()->GetSettings().using_synchronous_renderer_compositor) {
    // Root frame in Android WebView uses system scrollbars, so make ours
    // invisible. TODO(crbug.com/40226034): We should apply this to the root
    // scrollbars only, or consider other choices listed in the bug.
    color = SkColors::kTransparent;
  }

  if (color != color_.Read(*this)) {
    color_.Write(*this) = color;
    ScrollbarLayerBase::SetNeedsDisplayRect(gfx::Rect(bounds()));
    SetNeedsCommit();
  }
}

}  // namespace cc
