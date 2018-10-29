// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_scrollbar_layer.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "cc/base/math_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"

namespace {
static constexpr int kMaxScrollbarDimension = 8192;
};

namespace cc {

std::unique_ptr<LayerImpl> PaintedScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PaintedScrollbarLayerImpl::Create(
      tree_impl, id(), scrollbar_->Orientation(),
      scrollbar_->IsLeftSideVerticalScrollbar(), scrollbar_->IsOverlay());
}

scoped_refptr<PaintedScrollbarLayer> PaintedScrollbarLayer::Create(
    std::unique_ptr<Scrollbar> scrollbar,
    ElementId scroll_element_id) {
  return base::WrapRefCounted(
      new PaintedScrollbarLayer(std::move(scrollbar), scroll_element_id));
}

PaintedScrollbarLayer::PaintedScrollbarLayer(
    std::unique_ptr<Scrollbar> scrollbar,
    ElementId scroll_element_id)
    : scrollbar_(std::move(scrollbar)),
      scroll_element_id_(scroll_element_id),
      internal_contents_scale_(1.f),
      thumb_thickness_(scrollbar_->ThumbThickness()),
      thumb_length_(scrollbar_->ThumbLength()),
      is_overlay_(scrollbar_->IsOverlay()),
      has_thumb_(scrollbar_->HasThumb()),
      thumb_opacity_(scrollbar_->ThumbOpacity()) {
  if (!scrollbar_->IsOverlay()) {
    AddMainThreadScrollingReasons(
        MainThreadScrollingReason::kScrollbarScrolling);
  }
  SetIsScrollbar(true);
}

PaintedScrollbarLayer::~PaintedScrollbarLayer() = default;

void PaintedScrollbarLayer::SetScrollElementId(ElementId element_id) {
  if (element_id == scroll_element_id_)
    return;

  scroll_element_id_ = element_id;
  SetNeedsCommit();
}

bool PaintedScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return scrollbar_->IsOverlay();
}

void PaintedScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);

  PaintedScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetScrollElementId(scroll_element_id_);
  scrollbar_layer->set_internal_contents_scale_and_bounds(
      internal_contents_scale_, internal_content_bounds_);

  scrollbar_layer->SetThumbThickness(thumb_thickness_);
  scrollbar_layer->SetThumbLength(thumb_length_);
  if (scrollbar_->Orientation() == HORIZONTAL) {
    scrollbar_layer->SetTrackStart(
        track_rect_.x() - location_.x());
    scrollbar_layer->SetTrackLength(track_rect_.width());
  } else {
    scrollbar_layer->SetTrackStart(
        track_rect_.y() - location_.y());
    scrollbar_layer->SetTrackLength(track_rect_.height());
  }

  if (track_resource_.get())
    scrollbar_layer->set_track_ui_resource_id(track_resource_->id());
  else
    scrollbar_layer->set_track_ui_resource_id(0);
  if (thumb_resource_.get())
    scrollbar_layer->set_thumb_ui_resource_id(thumb_resource_->id());
  else
    scrollbar_layer->set_thumb_ui_resource_id(0);

  scrollbar_layer->set_thumb_opacity(thumb_opacity_);

  scrollbar_layer->set_is_overlay_scrollbar(is_overlay_);
}

void PaintedScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  // When the LTH is set to null or has changed, then this layer should remove
  // all of its associated resources.
  if (!host || host != layer_tree_host()) {
    track_resource_ = nullptr;
    thumb_resource_ = nullptr;
  }

  Layer::SetLayerTreeHost(host);
}

gfx::Rect PaintedScrollbarLayer::ScrollbarLayerRectToContentRect(
    const gfx::Rect& layer_rect) const {
  // Don't intersect with the bounds as in LayerRectToContentRect() because
  // layer_rect here might be in coordinates of the containing layer.
  gfx::Rect expanded_rect = gfx::ScaleToEnclosingRectSafe(
      layer_rect, internal_contents_scale_, internal_contents_scale_);
  // We should never return a rect bigger than the content bounds.
  gfx::Size clamped_size = expanded_rect.size();
  clamped_size.SetToMin(internal_content_bounds_);
  expanded_rect.set_size(clamped_size);
  return expanded_rect;
}

gfx::Rect PaintedScrollbarLayer::OriginThumbRect() const {
  gfx::Size thumb_size;
  if (scrollbar_->Orientation() == HORIZONTAL) {
    thumb_size =
        gfx::Size(scrollbar_->ThumbLength(), scrollbar_->ThumbThickness());
  } else {
    thumb_size =
        gfx::Size(scrollbar_->ThumbThickness(), scrollbar_->ThumbLength());
  }
  return gfx::Rect(thumb_size);
}

void PaintedScrollbarLayer::UpdateThumbAndTrackGeometry() {
  UpdateProperty(scrollbar_->TrackRect(), &track_rect_);
  UpdateProperty(scrollbar_->Location(), &location_);
  UpdateProperty(scrollbar_->IsOverlay(), &is_overlay_);
  UpdateProperty(scrollbar_->HasThumb(), &has_thumb_);
  if (has_thumb_) {
    UpdateProperty(scrollbar_->ThumbThickness(), &thumb_thickness_);
    UpdateProperty(scrollbar_->ThumbLength(), &thumb_length_);
  } else {
    UpdateProperty(0, &thumb_thickness_);
    UpdateProperty(0, &thumb_length_);
  }
}

void PaintedScrollbarLayer::UpdateInternalContentScale() {
  float scale = layer_tree_host()->device_scale_factor();
  if (layer_tree_host()
          ->GetSettings()
          .layer_transforms_should_scale_layer_contents) {
    gfx::Transform transform;
    transform = draw_property_utils::ScreenSpaceTransform(
        this, layer_tree_host()->property_trees()->transform_tree);

    gfx::Vector2dF transform_scales =
        MathUtil::ComputeTransform2dScaleComponents(transform, scale);
    scale = std::max(transform_scales.x(), transform_scales.y());
  }
  bool changed = false;
  changed |= UpdateProperty(scale, &internal_contents_scale_);
  changed |=
      UpdateProperty(gfx::ScaleToCeiledSize(bounds(), internal_contents_scale_),
                     &internal_content_bounds_);
  if (changed) {
    // If the content scale or bounds change, repaint.
    SetNeedsDisplay();
  }
}

bool PaintedScrollbarLayer::Update() {
  {
    base::AutoReset<bool> ignore_set_needs_commit(&ignore_set_needs_commit_,
                                                  true);
    Layer::Update();
    UpdateInternalContentScale();
  }

  UpdateThumbAndTrackGeometry();

  gfx::Rect track_layer_rect = gfx::Rect(location_, bounds());
  gfx::Rect scaled_track_rect = ScrollbarLayerRectToContentRect(
      track_layer_rect);

  bool updated = false;

  if (scaled_track_rect.IsEmpty()) {
    if (track_resource_) {
      track_resource_ = nullptr;
      thumb_resource_ = nullptr;
      SetNeedsPushProperties();
      updated = true;
    }
    return updated;
  }

  if (!has_thumb_ && thumb_resource_) {
    thumb_resource_ = nullptr;
    SetNeedsPushProperties();
    updated = true;
  }

  if (update_rect().IsEmpty() && track_resource_)
    return updated;

  if (!track_resource_ || scrollbar_->NeedsPaintPart(TRACK)) {
    track_resource_ = ScopedUIResource::Create(
        layer_tree_host()->GetUIResourceManager(),
        RasterizeScrollbarPart(track_layer_rect, scaled_track_rect, TRACK));
  }

  gfx::Rect thumb_layer_rect = OriginThumbRect();
  gfx::Rect scaled_thumb_rect =
      ScrollbarLayerRectToContentRect(thumb_layer_rect);
  if (has_thumb_ && !scaled_thumb_rect.IsEmpty()) {
    if (!thumb_resource_ || scrollbar_->NeedsPaintPart(THUMB) ||
        scaled_thumb_rect.size() !=
            thumb_resource_->GetBitmap(0, false).GetSize()) {
      thumb_resource_ = ScopedUIResource::Create(
          layer_tree_host()->GetUIResourceManager(),
          RasterizeScrollbarPart(thumb_layer_rect, scaled_thumb_rect, THUMB));
    }
    thumb_opacity_ = scrollbar_->ThumbOpacity();
  }

  // UI resources changed so push properties is needed.
  SetNeedsPushProperties();
  updated = true;
  return updated;
}

UIResourceBitmap PaintedScrollbarLayer::RasterizeScrollbarPart(
    const gfx::Rect& layer_rect,
    const gfx::Rect& requested_content_rect,
    ScrollbarPart part) {
  DCHECK(!requested_content_rect.size().IsEmpty());
  DCHECK(!layer_rect.size().IsEmpty());

  gfx::Rect content_rect = requested_content_rect;

  // Pages can end up requesting arbitrarily large scrollbars.  Prevent this
  // from crashing due to OOM and try something smaller.
  SkBitmap skbitmap;
  if (!skbitmap.tryAllocN32Pixels(content_rect.width(),
                                  content_rect.height())) {
    content_rect.Intersect(
        gfx::Rect(requested_content_rect.x(), requested_content_rect.y(),
                  kMaxScrollbarDimension, kMaxScrollbarDimension));
    skbitmap.allocN32Pixels(content_rect.width(), content_rect.height());
  }
  SkiaPaintCanvas canvas(skbitmap);
  canvas.clear(SK_ColorTRANSPARENT);

  float scale_x =
      content_rect.width() / static_cast<float>(layer_rect.width());
  float scale_y =
      content_rect.height() / static_cast<float>(layer_rect.height());
  canvas.scale(SkFloatToScalar(scale_x), SkFloatToScalar(scale_y));
  // TODO(pdr): Scrollbars are painted with an offset (see Scrollbar::PaintPart)
  // and the canvas is translated so that scrollbars are drawn at the origin.
  // Refactor this code to not use an offset at all so Scrollbar::PaintPart
  // paints at the origin and no translation is needed below.
  canvas.translate(-layer_rect.x(), -layer_rect.y());

  scrollbar_->PaintPart(&canvas, part, layer_rect);
  // Make sure that the pixels are no longer mutable to unavoid unnecessary
  // allocation and copying.
  skbitmap.setImmutable();

  return UIResourceBitmap(skbitmap);
}

}  // namespace cc
