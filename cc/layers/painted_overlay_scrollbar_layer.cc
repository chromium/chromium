// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_overlay_scrollbar_layer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "cc/base/math_util.h"
#include "cc/layers/painted_overlay_scrollbar_layer_impl.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

std::unique_ptr<LayerImpl> PaintedOverlayScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return PaintedOverlayScrollbarLayerImpl::Create(
      tree_impl, id(), orientation(), is_left_side_vertical_scrollbar());
}

scoped_refptr<PaintedOverlayScrollbarLayer>
PaintedOverlayScrollbarLayer::CreateOrReuse(
    scoped_refptr<Scrollbar> scrollbar,
    PaintedOverlayScrollbarLayer* existing_layer) {
  if (existing_layer &&
      existing_layer->scrollbar_.Read(*existing_layer)->IsSame(*scrollbar))
    return existing_layer;
  return Create(std::move(scrollbar));
}

scoped_refptr<PaintedOverlayScrollbarLayer>
PaintedOverlayScrollbarLayer::Create(scoped_refptr<Scrollbar> scrollbar) {
  return base::WrapRefCounted(
      new PaintedOverlayScrollbarLayer(std::move(scrollbar)));
}

PaintedOverlayScrollbarLayer::PaintedOverlayScrollbarLayer(
    scoped_refptr<Scrollbar> scrollbar)
    : ScrollbarLayerBase(scrollbar->Orientation(),
                         scrollbar->IsLeftSideVerticalScrollbar()),
      scrollbar_(std::move(scrollbar)) {
  DCHECK(scrollbar_.Read(*this)->HasThumb());
  DCHECK(scrollbar_.Read(*this)->IsOverlay());
  DCHECK(scrollbar_.Read(*this)->UsesNinePatchThumbResource());
}

PaintedOverlayScrollbarLayer::~PaintedOverlayScrollbarLayer() = default;

bool PaintedOverlayScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return true;
}

void PaintedOverlayScrollbarLayer::PushPropertiesTo(
    LayerImpl* layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  ScrollbarLayerBase::PushPropertiesTo(layer, commit_state, unsafe_state);

  PaintedOverlayScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedOverlayScrollbarLayerImpl*>(layer);

  if (orientation() == ScrollbarOrientation::kHorizontal) {
    scrollbar_layer->SetThumbThickness(thumb_size_.Read(*this).height());
    scrollbar_layer->SetThumbLength(thumb_size_.Read(*this).width());
    scrollbar_layer->SetTrackStart(track_rect_.Read(*this).x());
    scrollbar_layer->SetTrackLength(track_rect_.Read(*this).width());
  } else {
    scrollbar_layer->SetThumbThickness(thumb_size_.Read(*this).width());
    scrollbar_layer->SetThumbLength(thumb_size_.Read(*this).height());
    scrollbar_layer->SetTrackStart(track_rect_.Read(*this).y());
    scrollbar_layer->SetTrackLength(track_rect_.Read(*this).height());
  }

  if (thumb_resource_.Read(*this)) {
    auto iter =
        commit_state.ui_resource_sizes.find(thumb_resource_.Read(*this)->id());
    gfx::Size image_bounds = (iter == commit_state.ui_resource_sizes.end())
                                 ? gfx::Size()
                                 : iter->second;
    scrollbar_layer->SetImageBounds(image_bounds);
    scrollbar_layer->SetAperture(aperture_.Read(*this));
    scrollbar_layer->set_thumb_ui_resource_id(
        thumb_resource_.Read(*this)->id());
  } else {
    scrollbar_layer->SetImageBounds(gfx::Size());
    scrollbar_layer->SetAperture(gfx::Rect());
    scrollbar_layer->set_thumb_ui_resource_id(0);
  }

  if (track_resource_.Read(*this))
    scrollbar_layer->set_track_ui_resource_id(
        track_resource_.Read(*this)->id());
  else
    scrollbar_layer->set_track_ui_resource_id(0);
}

void PaintedOverlayScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  // When the LTH is set to null or has changed, then this layer should remove
  // all of its associated resources.
  if (host != layer_tree_host()) {
    thumb_resource_.Write(*this).reset();
    track_resource_.Write(*this).reset();
  }

  ScrollbarLayerBase::SetLayerTreeHost(host);
}

bool PaintedOverlayScrollbarLayer::Update() {
  // These properties should never change.
  DCHECK_EQ(orientation(), scrollbar_.Read(*this)->Orientation());
  DCHECK_EQ(is_left_side_vertical_scrollbar(),
            scrollbar_.Read(*this)->IsLeftSideVerticalScrollbar());
  DCHECK(scrollbar_.Read(*this)->HasThumb());
  DCHECK(scrollbar_.Read(*this)->IsOverlay());
  DCHECK(scrollbar_.Read(*this)->UsesNinePatchThumbResource());

  bool updated = false;
  updated |= Layer::Update();

  updated |= UpdateProperty(scrollbar_.Read(*this)->TrackRect(),
                            &track_rect_.Write(*this));
  // Ignore ThumbRect's location because the PaintedOverlayScrollbarLayerImpl
  // will compute it from scroll offset.
  updated |= UpdateProperty(scrollbar_.Read(*this)->ThumbRect().size(),
                            &thumb_size_.Write(*this));
  updated |= PaintThumbIfNeeded();
  updated |= SetHasFindInPageTickmarks(scrollbar_.Read(*this)->HasTickmarks());
  updated |= PaintTickmarks();

  return updated;
}

bool PaintedOverlayScrollbarLayer::PaintThumbIfNeeded() {
  auto& scrollbar = scrollbar_.Read(*this);
  if (!scrollbar->NeedsRepaintPart(ScrollbarPart::kThumb) &&
      thumb_resource_.Read(*this)) {
    return false;
  }

  gfx::Size paint_size = scrollbar->NinePatchThumbCanvasSize();
  DCHECK(!paint_size.IsEmpty());
  aperture_.Write(*this) = scrollbar->NinePatchThumbAperture();

  SkBitmap skbitmap;
  skbitmap.allocN32Pixels(paint_size.width(), paint_size.height());
  SkiaPaintCanvas canvas(skbitmap);
  canvas.clear(SkColors::kTransparent);

  scrollbar->PaintPart(&canvas, ScrollbarPart::kThumb, gfx::Rect(paint_size));
  // Make sure that the pixels are no longer mutable to unavoid unnecessary
  // allocation and copying.
  skbitmap.setImmutable();

  thumb_resource_.Write(*this) = ScopedUIResource::Create(
      layer_tree_host()->GetUIResourceManager(), UIResourceBitmap(skbitmap));

  SetNeedsPushProperties();

  return true;
}

bool PaintedOverlayScrollbarLayer::PaintTickmarks() {
  if (!has_find_in_page_tickmarks()) {
    if (!track_resource_.Read(*this)) {
      return false;
    } else {
      // Remove previous tickmarks.
      track_resource_.Write(*this).reset();
      SetNeedsPushProperties();
      return true;
    }
  }

  gfx::Size paint_size = track_rect_.Read(*this).size();
  DCHECK(!paint_size.IsEmpty());

  SkBitmap skbitmap;
  skbitmap.allocN32Pixels(paint_size.width(), paint_size.height());
  SkiaPaintCanvas canvas(skbitmap);
  canvas.clear(SkColors::kTransparent);

  scrollbar_.Write(*this)->PaintPart(
      &canvas, ScrollbarPart::kTrackButtonsTickmarks, gfx::Rect(paint_size));
  // Make sure that the pixels are no longer mutable to unavoid unnecessary
  // allocation and copying.
  skbitmap.setImmutable();

  track_resource_.Write(*this) = ScopedUIResource::Create(
      layer_tree_host()->GetUIResourceManager(), UIResourceBitmap(skbitmap));

  SetNeedsPushProperties();
  return true;
}

ScrollbarLayerBase::ScrollbarLayerType
PaintedOverlayScrollbarLayer::GetScrollbarLayerType() const {
  return kPaintedOverlay;
}

}  // namespace cc
