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
      track_start_(TrackStart(*scrollbar_.Read(*this))) {
  DCHECK(scrollbar_.Read(*this)->IsOverlay());
  DCHECK(scrollbar_.Read(*this)->IsSolidColor());
  Layer::SetOpacity(0.f);
}

SolidColorScrollbarLayer::~SolidColorScrollbarLayer() = default;

void SolidColorScrollbarLayer::SetOpacity(float opacity) {
  // The opacity of a solid color scrollbar layer is always 0 on main thread.
  DCHECK_EQ(opacity, 0.f);
  Layer::SetOpacity(opacity);
}

bool SolidColorScrollbarLayer::Update() {
  bool updated = ScrollbarLayerBase::Update();
  Scrollbar* scrollbar = scrollbar_.Write(*this).get();
  if (scrollbar->ThumbNeedsRepaint() || !color_.Read(*this).has_value()) {
    SkColor4f color = scrollbar->ThumbColor();
    if (layer_tree_host() && layer_tree_host()
                                 ->GetSettings()
                                 .using_synchronous_renderer_compositor) {
      // Root frame in Android WebView uses system scrollbars, so make ours
      // invisible. TODO(crbug.com/40226034): We should apply this to the root
      // scrollbars only, or consider other choices listed in the bug.
      color = SkColors::kTransparent;
    }
    scrollbar->ClearThumbNeedsRepaint();

    if (color != color_.Read(*this)) {
      color_.Write(*this) = color;
      SetNeedsPushProperties();
      updated = true;
    }
  }
  return updated;
}

void SolidColorScrollbarLayer::SetNeedsDisplayRect(const gfx::Rect& rect) {
  // Solid color scrollbars do not rasterize on the main thread, and thumb
  // geometry is computed on the impl thread. Property changes and damage are
  // tracked on the impl thread via SolidColorScrollbarLayerImpl::set_color(),
  // so main-thread display invalidation (update_rect_) is unnecessary.
  // However, we must ensure Update() is invoked to pull the new thumb color
  // during frame production if the thumb needs repaint or color has not been
  // initialized.
  const Scrollbar* scrollbar = scrollbar_.Read(*this).get();
  if (layer_tree_host() && !rect.IsEmpty() &&
      (scrollbar->ThumbNeedsRepaint() || !color_.Read(*this).has_value())) {
    layer_tree_host()->SetNeedsUpdateLayers();
  }
}

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
    static_cast<SolidColorScrollbarLayerImpl*>(layer)->set_color(
        color_.Read(*this).value_or(SkColors::kTransparent));
  }
}

void SolidColorScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (host != layer_tree_host()) {
    ScrollbarLayerBase::SetLayerTreeHost(host);
    // Reset the cached color so that Update() on the new host will pull the
    // thumb color even if ThumbNeedsRepaint() was already cleared on the
    // previous host.
    color_.Write(*this) = std::nullopt;
    if (host) {
      host->SetNeedsUpdateLayers();
    }
  }
}

}  // namespace cc
