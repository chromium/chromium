// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_overlay_scrollbar_layer.h"

#include <algorithm>

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
#include "ui/gfx/skia_util.h"

namespace cc {

std::unique_ptr<LayerImpl> PaintedOverlayScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PaintedOverlayScrollbarLayerImpl::Create(
      tree_impl, id(), orientation(), is_left_side_vertical_scrollbar());
}

scoped_refptr<PaintedOverlayScrollbarLayer>
PaintedOverlayScrollbarLayer::CreateOrReuse(
    scoped_refptr<Scrollbar> scrollbar,
    PaintedOverlayScrollbarLayer* existing_layer) {
  if (existing_layer && existing_layer->scrollbar_->IsSame(*scrollbar))
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
  DCHECK(scrollbar_->HasThumb());
  DCHECK(scrollbar_->IsOverlay());
  DCHECK(scrollbar_->UsesNinePatchThumbResource());
}

PaintedOverlayScrollbarLayer::~PaintedOverlayScrollbarLayer() = default;

bool PaintedOverlayScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return true;
}

void PaintedOverlayScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  ScrollbarLayerBase::PushPropertiesTo(layer);

  PaintedOverlayScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedOverlayScrollbarLayerImpl*>(layer);

  if (orientation() == ScrollbarOrientation::HORIZONTAL) {
    scrollbar_layer->SetThumbThickness(thumb_size_.height());
    scrollbar_layer->SetThumbLength(thumb_size_.width());
    scrollbar_layer->SetTrackStart(track_rect_.x());
    scrollbar_layer->SetTrackLength(track_rect_.width());
  } else {
    scrollbar_layer->SetThumbThickness(thumb_size_.width());
    scrollbar_layer->SetThumbLength(thumb_size_.height());
    scrollbar_layer->SetTrackStart(track_rect_.y());
    scrollbar_layer->SetTrackLength(track_rect_.height());
  }

  if (thumb_resource_.get()) {
    scrollbar_layer->SetImageBounds(
        layer_tree_host()->GetUIResourceManager()->GetUIResourceSize(
            thumb_resource_->id()));
    scrollbar_layer->SetAperture(aperture_);
    scrollbar_layer->set_thumb_ui_resource_id(thumb_resource_->id());
  } else {
    scrollbar_layer->SetImageBounds(gfx::Size());
    scrollbar_layer->SetAperture(gfx::Rect());
    scrollbar_layer->set_thumb_ui_resource_id(0);
  }

  if (track_resource_.get())
    scrollbar_layer->set_track_ui_resource_id(track_resource_->id());
  else
    scrollbar_layer->set_track_ui_resource_id(0);
}

void PaintedOverlayScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  // When the LTH is set to null or has changed, then this layer should remove
  // all of its associated resources.
  if (host != layer_tree_host()) {
    thumb_resource_.reset();
    track_resource_.reset();
  }

  ScrollbarLayerBase::SetLayerTreeHost(host);
}

bool PaintedOverlayScrollbarLayer::Update() {
  // These properties should never change.
  DCHECK_EQ(orientation(), scrollbar_->Orientation());
  DCHECK_EQ(is_left_side_vertical_scrollbar(),
            scrollbar_->IsLeftSideVerticalScrollbar());
  DCHECK(scrollbar_->HasThumb());
  DCHECK(scrollbar_->IsOverlay());
  DCHECK(scrollbar_->UsesNinePatchThumbResource());

  bool updated = false;
  updated |= Layer::Update();

  updated |= UpdateProperty(scrollbar_->TrackRect(), &track_rect_);
  // Ignore ThumbRect's location because the PaintedOverlayScrollbarLayerImpl
  // will compute it from scroll offset.
  updated |= UpdateProperty(scrollbar_->ThumbRect().size(), &thumb_size_);
  updated |= PaintThumbIfNeeded();
  updated |= PaintTickmarks();

  return updated;
}

bool PaintedOverlayScrollbarLayer::PaintThumbIfNeeded() {
  if (!scrollbar_->NeedsRepaintPart(ScrollbarPart::THUMB) && thumb_resource_)
    return false;

  gfx::Size paint_size = scrollbar_->NinePatchThumbCanvasSize();
  DCHECK(!paint_size.IsEmpty());
  aperture_ = scrollbar_->NinePatchThumbAperture();

  SkBitmap skbitmap;
  skbitmap.allocN32Pixels(paint_size.width(), paint_size.height());
  SkiaPaintCanvas canvas(skbitmap);
  canvas.clear(SK_ColorTRANSPARENT);

  scrollbar_->PaintPart(&canvas, ScrollbarPart::THUMB, gfx::Rect(paint_size));
  // Make sure that the pixels are no longer mutable to unavoid unnecessary
  // allocation and copying.
  skbitmap.setImmutable();

  thumb_resource_ = ScopedUIResource::Create(
      layer_tree_host()->GetUIResourceManager(), UIResourceBitmap(skbitmap));

  SetNeedsPushProperties();

  return true;
}

bool PaintedOverlayScrollbarLayer::PaintTickmarks() {
  if (!scrollbar_->HasTickmarks()) {
    if (!track_resource_) {
      return false;
    } else {
      // Remove previous tickmarks.
      track_resource_.reset();
      SetNeedsPushProperties();
      return true;
    }
  }

  gfx::Size paint_size = track_rect_.size();
  DCHECK(!paint_size.IsEmpty());

  SkBitmap skbitmap;
  skbitmap.allocN32Pixels(paint_size.width(), paint_size.height());
  SkiaPaintCanvas canvas(skbitmap);
  canvas.clear(SK_ColorTRANSPARENT);

  scrollbar_->PaintPart(&canvas, ScrollbarPart::TRACK_BUTTONS_TICKMARKS,
                        gfx::Rect(paint_size));
  // Make sure that the pixels are no longer mutable to unavoid unnecessary
  // allocation and copying.
  skbitmap.setImmutable();

  track_resource_ = ScopedUIResource::Create(
      layer_tree_host()->GetUIResourceManager(), UIResourceBitmap(skbitmap));

  SetNeedsPushProperties();
  return true;
}

ScrollbarLayerBase::ScrollbarLayerType
PaintedOverlayScrollbarLayer::GetScrollbarLayerType() const {
  return kPaintedOverlay;
}

}  // namespace cc
