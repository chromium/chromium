// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_scrollbar_layer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

std::unique_ptr<LayerImpl> PaintedScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PaintedScrollbarLayerImpl::Create(tree_impl, id(), orientation(),
                                           is_left_side_vertical_scrollbar(),
                                           is_overlay_);
}

scoped_refptr<PaintedScrollbarLayer> PaintedScrollbarLayer::CreateOrReuse(
    scoped_refptr<Scrollbar> scrollbar,
    PaintedScrollbarLayer* existing_layer) {
  if (existing_layer && existing_layer->scrollbar_->IsSame(*scrollbar))
    return existing_layer;
  return Create(std::move(scrollbar));
}

scoped_refptr<PaintedScrollbarLayer> PaintedScrollbarLayer::Create(
    scoped_refptr<Scrollbar> scrollbar) {
  return base::WrapRefCounted(new PaintedScrollbarLayer(std::move(scrollbar)));
}

PaintedScrollbarLayer::PaintedScrollbarLayer(scoped_refptr<Scrollbar> scrollbar)
    : ScrollbarLayerBase(scrollbar->Orientation(),
                         scrollbar->IsLeftSideVerticalScrollbar()),
      scrollbar_(std::move(scrollbar)),
      internal_contents_scale_(1.f),
      painted_opacity_(scrollbar_->Opacity()),
      has_thumb_(scrollbar_->HasThumb()),
      jump_on_track_click_(scrollbar_->JumpOnTrackClick()),
      supports_drag_snap_back_(scrollbar_->SupportsDragSnapBack()),
      is_overlay_(scrollbar_->IsOverlay()) {}

PaintedScrollbarLayer::~PaintedScrollbarLayer() = default;

bool PaintedScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return is_overlay_;
}

void PaintedScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  ScrollbarLayerBase::PushPropertiesTo(layer);

  PaintedScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedScrollbarLayerImpl*>(layer);

  scrollbar_layer->set_internal_contents_scale_and_bounds(
      internal_contents_scale_, internal_content_bounds_);

  scrollbar_layer->SetJumpOnTrackClick(jump_on_track_click_);
  scrollbar_layer->SetSupportsDragSnapBack(supports_drag_snap_back_);
  scrollbar_layer->SetBackButtonRect(back_button_rect_);
  scrollbar_layer->SetForwardButtonRect(forward_button_rect_);
  scrollbar_layer->SetTrackRect(track_rect_);
  if (orientation() == ScrollbarOrientation::HORIZONTAL) {
    scrollbar_layer->SetThumbThickness(thumb_size_.height());
    scrollbar_layer->SetThumbLength(thumb_size_.width());
  } else {
    scrollbar_layer->SetThumbThickness(thumb_size_.width());
    scrollbar_layer->SetThumbLength(thumb_size_.height());
  }

  if (track_resource_.get())
    scrollbar_layer->set_track_ui_resource_id(track_resource_->id());
  else
    scrollbar_layer->set_track_ui_resource_id(0);
  if (thumb_resource_.get())
    scrollbar_layer->set_thumb_ui_resource_id(thumb_resource_->id());
  else
    scrollbar_layer->set_thumb_ui_resource_id(0);

  scrollbar_layer->set_scrollbar_painted_opacity(painted_opacity_);

  scrollbar_layer->set_is_overlay_scrollbar(is_overlay_);
}

void PaintedScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  // When the LTH is set to null or has changed, then this layer should remove
  // all of its associated resources.
  if (!host || host != layer_tree_host()) {
    track_resource_ = nullptr;
    thumb_resource_ = nullptr;
  }

  ScrollbarLayerBase::SetLayerTreeHost(host);
}

gfx::Size PaintedScrollbarLayer::LayerSizeToContentSize(
    const gfx::Size& layer_size) const {
  gfx::Size content_size =
      gfx::ScaleToCeiledSize(layer_size, internal_contents_scale_);
  // We should never return a rect bigger than the content bounds.
  content_size.SetToMin(internal_content_bounds_);
  return content_size;
}

bool PaintedScrollbarLayer::UpdateThumbAndTrackGeometry() {
  // These properties should never change.
  DCHECK_EQ(supports_drag_snap_back_, scrollbar_->SupportsDragSnapBack());
  DCHECK_EQ(is_left_side_vertical_scrollbar(),
            scrollbar_->IsLeftSideVerticalScrollbar());
  DCHECK_EQ(is_overlay_, scrollbar_->IsOverlay());
  DCHECK_EQ(orientation(), scrollbar_->Orientation());

  bool updated = false;
  updated |=
      UpdateProperty(scrollbar_->JumpOnTrackClick(), &jump_on_track_click_);
  updated |= UpdateProperty(scrollbar_->TrackRect(), &track_rect_);
  updated |= UpdateProperty(scrollbar_->BackButtonRect(), &back_button_rect_);
  updated |=
      UpdateProperty(scrollbar_->ForwardButtonRect(), &forward_button_rect_);
  updated |= UpdateProperty(scrollbar_->HasThumb(), &has_thumb_);
  if (has_thumb_) {
    // Ignore ThumbRect's location because the PaintedScrollbarLayerImpl will
    // compute it from scroll offset.
    updated |= UpdateProperty(scrollbar_->ThumbRect().size(), &thumb_size_);
  } else {
    updated |= UpdateProperty(gfx::Size(), &thumb_size_);
  }
  return updated;
}

bool PaintedScrollbarLayer::UpdateInternalContentScale() {
  gfx::Transform transform;
  transform = draw_property_utils::ScreenSpaceTransform(
      this, layer_tree_host()->property_trees()->transform_tree);

  gfx::Vector2dF transform_scales = MathUtil::ComputeTransform2dScaleComponents(
      transform, layer_tree_host()->device_scale_factor());
  float scale = std::max(transform_scales.x(), transform_scales.y());
  // Clamp minimum scale to 1 to avoid too low scale during scale animation.
  // TODO(crbug.com/1009291): Move rasterization of scrollbars to the impl side
  // to better handle scale changes.
  scale = std::max(1.0f, scale);

  bool updated = false;
  updated |= UpdateProperty(scale, &internal_contents_scale_);
  updated |=
      UpdateProperty(gfx::ScaleToCeiledSize(bounds(), internal_contents_scale_),
                     &internal_content_bounds_);
  return updated;
}

bool PaintedScrollbarLayer::Update() {
  bool updated = false;

  updated |= ScrollbarLayerBase::Update();
  updated |= UpdateInternalContentScale();
  updated |= UpdateThumbAndTrackGeometry();

  gfx::Size size = bounds();
  gfx::Size scaled_size = internal_content_bounds_;

  if (scaled_size.IsEmpty()) {
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

  if (!track_resource_ ||
      scrollbar_->NeedsRepaintPart(ScrollbarPart::TRACK_BUTTONS_TICKMARKS)) {
    track_resource_ = ScopedUIResource::Create(
        layer_tree_host()->GetUIResourceManager(),
        RasterizeScrollbarPart(size, scaled_size,
                               ScrollbarPart::TRACK_BUTTONS_TICKMARKS));
    SetNeedsPushProperties();
    updated = true;
  }

  gfx::Size scaled_thumb_size = LayerSizeToContentSize(thumb_size_);
  if (has_thumb_ && !scaled_thumb_size.IsEmpty()) {
    if (!thumb_resource_ ||
        scrollbar_->NeedsRepaintPart(ScrollbarPart::THUMB) ||
        scaled_thumb_size != thumb_resource_->GetBitmap(0, false).GetSize()) {
      thumb_resource_ = ScopedUIResource::Create(
          layer_tree_host()->GetUIResourceManager(),
          RasterizeScrollbarPart(thumb_size_, scaled_thumb_size,
                                 ScrollbarPart::THUMB));
      SetNeedsPushProperties();
      updated = true;
    }
    updated |= UpdateProperty(scrollbar_->Opacity(), &painted_opacity_);
  }

  return updated;
}

UIResourceBitmap PaintedScrollbarLayer::RasterizeScrollbarPart(
    const gfx::Size& size,
    const gfx::Size& requested_content_size,
    ScrollbarPart part) {
  DCHECK(!requested_content_size.IsEmpty());
  DCHECK(!size.IsEmpty());

  gfx::Size content_size = requested_content_size;

  // Pages can end up requesting arbitrarily large scrollbars.  Prevent this
  // from crashing due to OOM and try something smaller.
  SkBitmap skbitmap;
  bool allocation_succeeded =
      skbitmap.tryAllocN32Pixels(content_size.width(), content_size.height());
  // Assuming 4bpp, caps at 4M.
  constexpr int kMinScrollbarDimension = 1024;
  int dimension = std::max(content_size.width(), content_size.height()) / 2;
  while (!allocation_succeeded && dimension >= kMinScrollbarDimension) {
    content_size.SetToMin(gfx::Size(dimension, dimension));
    allocation_succeeded =
        skbitmap.tryAllocN32Pixels(content_size.width(), content_size.height());
    if (!allocation_succeeded)
      dimension = dimension / 2;
  }
  CHECK(allocation_succeeded)
      << "Failed to allocate memory for scrollbar at dimension : " << dimension;

  SkiaPaintCanvas canvas(skbitmap);
  canvas.clear(SK_ColorTRANSPARENT);

  float scale_x = content_size.width() / static_cast<float>(size.width());
  float scale_y = content_size.height() / static_cast<float>(size.height());
  canvas.scale(SkFloatToScalar(scale_x), SkFloatToScalar(scale_y));

  scrollbar_->PaintPart(&canvas, part, gfx::Rect(size));
  // Make sure that the pixels are no longer mutable to unavoid unnecessary
  // allocation and copying.
  skbitmap.setImmutable();

  return UIResourceBitmap(skbitmap);
}

ScrollbarLayerBase::ScrollbarLayerType
PaintedScrollbarLayer::GetScrollbarLayerType() const {
  return kPainted;
}

}  // namespace cc
