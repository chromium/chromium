// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_scrollbar_layer.h"

#include <memory>
#include <utility>

#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

namespace {

int ThumbThickness(const Scrollbar& scrollbar) {
  gfx::Rect thumb_rect = scrollbar.ThumbRect();
  return scrollbar.Orientation() == ScrollbarOrientation::kHorizontal
             ? thumb_rect.height()
             : thumb_rect.width();
}

int TrackStart(const Scrollbar& scrollbar) {
  gfx::Rect track_rect = scrollbar.TrackRect();
  return scrollbar.Orientation() == ScrollbarOrientation::kHorizontal
             ? track_rect.x()
             : track_rect.y();
}

}  // namespace

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
  int thumb_thickness = ThumbThickness(*scrollbar);
  int track_start = TrackStart(*scrollbar);

  if (existing_layer &&
      existing_layer->scrollbar_.Read(*existing_layer)->IsSame(*scrollbar) &&
      // We don't support change of these fields in a layer.
      existing_layer->thumb_thickness() == thumb_thickness &&
      existing_layer->track_start() == track_start) {
    // These fields have been checked in ScrollbarLayerBase::CreateOrReuse().
    DCHECK_EQ(scrollbar->Orientation(), existing_layer->orientation());
    DCHECK_EQ(scrollbar->IsLeftSideVerticalScrollbar(),
              existing_layer->is_left_side_vertical_scrollbar());
    existing_layer->SetColor(scrollbar->ThumbColor());
    return existing_layer;
  }

  return Create(std::move(scrollbar));
}

scoped_refptr<SolidColorScrollbarLayer> SolidColorScrollbarLayer::Create(
    scoped_refptr<Scrollbar> scrollbar) {
  return base::WrapRefCounted(
      new SolidColorScrollbarLayer(std::move(scrollbar)));
}

SolidColorScrollbarLayer::SolidColorScrollbarLayer(
    scoped_refptr<Scrollbar> scrollbar)
    : ScrollbarLayerBase(scrollbar->Orientation(),
                         scrollbar->IsLeftSideVerticalScrollbar()),
      scrollbar_(std::move(scrollbar)),
      thumb_thickness_(ThumbThickness(*scrollbar_.Read(*this))),
      track_start_(TrackStart(*scrollbar_.Read(*this))),
      color_(SkColors::kTransparent) {
  DCHECK(scrollbar_.Read(*this)->IsOverlay());
  DCHECK(scrollbar_.Read(*this)->IsSolidColor());
  Layer::SetOpacity(0.f);
  SetColor(scrollbar_.Read(*this)->ThumbColor());
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

void SolidColorScrollbarLayer::PushDirtyPropertiesTo(
    LayerImpl* layer,
    uint8_t dirty_flag,
    CommitState& commit_state) {
  ScrollbarLayerBase::PushDirtyPropertiesTo(layer, dirty_flag, commit_state);

  if (dirty_flag & kChangedGeneralProperty) {
    static_cast<SolidColorScrollbarLayerImpl*>(layer)->set_color(color());
  }
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
