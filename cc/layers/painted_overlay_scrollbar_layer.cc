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
      tree_impl, id(), scrollbar_->Orientation(),
      scrollbar_->IsLeftSideVerticalScrollbar());
}

scoped_refptr<PaintedOverlayScrollbarLayer>
PaintedOverlayScrollbarLayer::Create(std::unique_ptr<Scrollbar> scrollbar,
                                     ElementId scroll_element_id) {
  return base::WrapRefCounted(new PaintedOverlayScrollbarLayer(
      std::move(scrollbar), scroll_element_id));
}

PaintedOverlayScrollbarLayer::PaintedOverlayScrollbarLayer(
    std::unique_ptr<Scrollbar> scrollbar,
    ElementId scroll_element_id)
    : scrollbar_(std::move(scrollbar)),
      scroll_element_id_(scroll_element_id),
      thumb_thickness_(scrollbar_->ThumbThickness()),
      thumb_length_(scrollbar_->ThumbLength()) {
  DCHECK(scrollbar_->UsesNinePatchThumbResource());
  SetIsScrollbar(true);
}

PaintedOverlayScrollbarLayer::~PaintedOverlayScrollbarLayer() = default;

void PaintedOverlayScrollbarLayer::SetScrollElementId(ElementId element_id) {
  if (element_id == scroll_element_id_)
    return;

  scroll_element_id_ = element_id;
  SetNeedsCommit();
}

bool PaintedOverlayScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return scrollbar_->IsOverlay();
}

void PaintedOverlayScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);

  PaintedOverlayScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedOverlayScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetScrollElementId(scroll_element_id_);

  scrollbar_layer->SetThumbThickness(thumb_thickness_);
  scrollbar_layer->SetThumbLength(thumb_length_);
  if (scrollbar_->Orientation() == HORIZONTAL) {
    scrollbar_layer->SetTrackStart(track_rect_.x() - location_.x());
    scrollbar_layer->SetTrackLength(track_rect_.width());
  } else {
    scrollbar_layer->SetTrackStart(track_rect_.y() - location_.y());
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

  Layer::SetLayerTreeHost(host);
}

gfx::Rect PaintedOverlayScrollbarLayer::OriginThumbRectForPainting() const {
  return gfx::Rect(gfx::Point(), scrollbar_->NinePatchThumbCanvasSize());
}

bool PaintedOverlayScrollbarLayer::Update() {
  bool updated = false;
  updated |= Layer::Update();

  DCHECK(scrollbar_->HasThumb());
  DCHECK(scrollbar_->IsOverlay());
  DCHECK(scrollbar_->UsesNinePatchThumbResource());
  updated |= UpdateProperty(scrollbar_->TrackRect(), &track_rect_);
  updated |= UpdateProperty(scrollbar_->Location(), &location_);
  updated |= UpdateProperty(scrollbar_->ThumbThickness(), &thumb_thickness_);
  updated |= UpdateProperty(scrollbar_->ThumbLength(), &thumb_length_);
  updated |= PaintThumbIfNeeded();
  updated |= PaintTickmarks();

  return updated;
}

bool PaintedOverlayScrollbarLayer::PaintThumbIfNeeded() {
  if (!scrollbar_->NeedsPaintPart(THUMB) && thumb_resource_)
    return false;

  gfx::Rect paint_rect = OriginThumbRectForPainting();
  aperture_ = scrollbar_->NinePatchThumbAperture();

  DCHECK(!paint_rect.size().IsEmpty());
  DCHECK(paint_rect.origin().IsOrigin());

  SkBitmap skbitmap;
  skbitmap.allocN32Pixels(paint_rect.width(), paint_rect.height());
  SkiaPaintCanvas canvas(skbitmap);

  SkRect content_skrect = RectToSkRect(paint_rect);
  PaintFlags flags;
  flags.setAntiAlias(false);
  flags.setBlendMode(SkBlendMode::kClear);
  canvas.drawRect(content_skrect, flags);
  canvas.clipRect(content_skrect);

  scrollbar_->PaintPart(&canvas, THUMB, paint_rect);
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

  gfx::Rect paint_rect = gfx::Rect(gfx::Point(), track_rect_.size());

  DCHECK(!paint_rect.size().IsEmpty());

  SkBitmap skbitmap;
  skbitmap.allocN32Pixels(paint_rect.width(), paint_rect.height());
  SkiaPaintCanvas canvas(skbitmap);

  SkRect content_skrect = RectToSkRect(paint_rect);
  PaintFlags flags;
  flags.setAntiAlias(false);
  flags.setBlendMode(SkBlendMode::kClear);
  canvas.drawRect(content_skrect, flags);
  canvas.clipRect(content_skrect);

  scrollbar_->PaintPart(&canvas, TICKMARKS, paint_rect);
  // Make sure that the pixels are no longer mutable to unavoid unnecessary
  // allocation and copying.
  skbitmap.setImmutable();

  track_resource_ = ScopedUIResource::Create(
      layer_tree_host()->GetUIResourceManager(), UIResourceBitmap(skbitmap));

  SetNeedsPushProperties();
  return true;
}

}  // namespace cc
