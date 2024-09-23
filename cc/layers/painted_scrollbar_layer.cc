// Copyright 2013 The Chromium Authors
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
#include "ui/gfx/geometry/transform_util.h"

namespace cc {

std::unique_ptr<LayerImpl> PaintedScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return PaintedScrollbarLayerImpl::Create(tree_impl, id(), orientation(),
                                           is_left_side_vertical_scrollbar(),
                                           is_overlay_);
}

scoped_refptr<PaintedScrollbarLayer> PaintedScrollbarLayer::CreateOrReuse(
    scoped_refptr<Scrollbar> scrollbar,
    PaintedScrollbarLayer* existing_layer) {
  if (existing_layer &&
      existing_layer->scrollbar_.Read(*existing_layer)->IsSame(*scrollbar))
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
      painted_opacity_(scrollbar_.Read(*this)->Opacity()),
      has_thumb_(scrollbar_.Read(*this)->HasThumb()),
      jump_on_track_click_(scrollbar_.Read(*this)->JumpOnTrackClick()),
      supports_drag_snap_back_(scrollbar_.Read(*this)->SupportsDragSnapBack()),
      is_overlay_(scrollbar_.Read(*this)->IsOverlay()),
      is_web_test_(scrollbar_.Read(*this)->IsRunningWebTest()),
      uses_nine_patch_track_and_buttons_(
          scrollbar_.Read(*this)->UsesNinePatchTrackAndButtonsResource()),
      uses_solid_color_thumb_(scrollbar_.Read(*this)->UsesSolidColorThumb()) {}

PaintedScrollbarLayer::~PaintedScrollbarLayer() = default;

bool PaintedScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return is_overlay_;
}

void PaintedScrollbarLayer::PushPropertiesTo(
    LayerImpl* layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  ScrollbarLayerBase::PushPropertiesTo(layer, commit_state, unsafe_state);

  PaintedScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedScrollbarLayerImpl*>(layer);

  scrollbar_layer->set_internal_contents_scale_and_bounds(
      internal_contents_scale_.Read(*this),
      internal_content_bounds_.Read(*this));

  scrollbar_layer->SetJumpOnTrackClick(jump_on_track_click_.Read(*this));
  scrollbar_layer->SetSupportsDragSnapBack(supports_drag_snap_back_);
  scrollbar_layer->SetBackButtonRect(back_button_rect_.Read(*this));
  scrollbar_layer->SetForwardButtonRect(forward_button_rect_.Read(*this));
  scrollbar_layer->SetTrackRect(track_rect_.Read(*this));
  if (orientation() == ScrollbarOrientation::kHorizontal) {
    scrollbar_layer->SetThumbThickness(thumb_size_.Read(*this).height());
    scrollbar_layer->SetThumbLength(thumb_size_.Read(*this).width());
  } else {
    scrollbar_layer->SetThumbThickness(thumb_size_.Read(*this).width());
    scrollbar_layer->SetThumbLength(thumb_size_.Read(*this).height());
  }

  if (track_and_buttons_resource_.Read(*this)) {
    scrollbar_layer->set_track_and_buttons_ui_resource_id(
        track_and_buttons_resource_.Read(*this)->id());
  } else {
    scrollbar_layer->set_track_and_buttons_ui_resource_id(0);
  }
  if (thumb_resource_.Read(*this)) {
    scrollbar_layer->set_thumb_ui_resource_id(
        thumb_resource_.Read(*this)->id());
  } else {
    scrollbar_layer->set_thumb_ui_resource_id(0);
  }

  scrollbar_layer->SetScrollbarPaintedOpacity(painted_opacity_.Read(*this));

  scrollbar_layer->set_is_overlay_scrollbar(is_overlay_);
  scrollbar_layer->set_is_web_test(is_web_test_);

  if (thumb_color_.Read(*this).has_value()) {
    scrollbar_layer->SetThumbColor(thumb_color_.Read(*this).value());
  }
  if (uses_nine_patch_track_and_buttons_ &&
      track_and_buttons_resource_.Read(*this)) {
    const auto iter = commit_state.ui_resource_sizes.find(
        track_and_buttons_resource_.Read(*this)->id());
    const gfx::Size image_bounds =
        (iter == commit_state.ui_resource_sizes.end()) ? gfx::Size()
                                                       : iter->second;
    scrollbar_layer->SetTrackAndButtonsImageBounds(image_bounds);
    scrollbar_layer->SetTrackAndButtonsAperture(
        track_and_buttons_aperture_.Read(*this));
  } else {
    scrollbar_layer->SetTrackAndButtonsImageBounds(gfx::Size());
    scrollbar_layer->SetTrackAndButtonsAperture(gfx::Rect());
  }
  scrollbar_layer->set_uses_nine_patch_track_and_buttons(
      uses_nine_patch_track_and_buttons_);
}

void PaintedScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  // When the LTH is set to null or has changed, then this layer should remove
  // all of its associated resources.
  if (!host || host != layer_tree_host()) {
    track_and_buttons_resource_.Write(*this) = nullptr;
    thumb_resource_.Write(*this) = nullptr;
  }

  ScrollbarLayerBase::SetLayerTreeHost(host);
}

gfx::Size PaintedScrollbarLayer::LayerSizeToContentSize(
    const gfx::Size& layer_size) const {
  gfx::Size content_size =
      gfx::ScaleToCeiledSize(layer_size, internal_contents_scale_.Read(*this));
  // We should never return a rect bigger than the content bounds.
  content_size.SetToMin(internal_content_bounds_.Read(*this));
  return content_size;
}

bool PaintedScrollbarLayer::UpdateGeometry() {
  // These properties should never change.
  DCHECK_EQ(supports_drag_snap_back_,
            scrollbar_.Read(*this)->SupportsDragSnapBack());
  DCHECK_EQ(is_left_side_vertical_scrollbar(),
            scrollbar_.Read(*this)->IsLeftSideVerticalScrollbar());
  DCHECK_EQ(is_overlay_, scrollbar_.Read(*this)->IsOverlay());
  DCHECK_EQ(orientation(), scrollbar_.Read(*this)->Orientation());

  bool updated = false;
  const auto& scrollbar = scrollbar_.Read(*this);
  updated |= UpdateProperty(scrollbar->JumpOnTrackClick(),
                            &jump_on_track_click_.Write(*this));
  updated |= UpdateProperty(scrollbar->TrackRect(), &track_rect_.Write(*this));
  updated |= UpdateProperty(scrollbar->BackButtonRect(),
                            &back_button_rect_.Write(*this));
  updated |= UpdateProperty(scrollbar->ForwardButtonRect(),
                            &forward_button_rect_.Write(*this));
  updated |= UpdateProperty(scrollbar->HasThumb(), &has_thumb_.Write(*this));
  if (has_thumb_.Read(*this)) {
    gfx::Rect thumb_rect = scrollbar->ThumbRect();
    if (uses_solid_color_thumb_) {
      thumb_rect.Inset(scrollbar->SolidColorThumbInsets());
    }
    // Ignore ThumbRect's location because the PaintedScrollbarLayerImpl will
    // compute it from scroll offset.
    updated |= UpdateProperty(thumb_rect.size(), &thumb_size_.Write(*this));
  } else {
    updated |= UpdateProperty(gfx::Size(), &thumb_size_.Write(*this));
  }
  return updated;
}

bool PaintedScrollbarLayer::UpdateInternalContentScale() {
  gfx::Transform transform;
  transform = draw_property_utils::ScreenSpaceTransform(
      this, layer_tree_host()->property_trees()->transform_tree());

  gfx::Vector2dF transform_scales = gfx::ComputeTransform2dScaleComponents(
      transform, layer_tree_host()->device_scale_factor());
  float scale = std::max(transform_scales.x(), transform_scales.y());
  // Clamp minimum scale to 1 to avoid too low scale during scale animation.
  // TODO(crbug.com/40100995): Move rasterization of scrollbars to the impl side
  // to better handle scale changes.
  scale = std::max(1.0f, scale);

  bool updated = false;
  updated |= UpdateProperty(scale, &internal_contents_scale_.Write(*this));
  updated |= UpdateProperty(
      gfx::ScaleToCeiledSize(bounds(), internal_contents_scale_.Read(*this)),
      &internal_content_bounds_.Write(*this));
  return updated;
}

bool PaintedScrollbarLayer::Update() {
  bool updated = false;

  updated |= ScrollbarLayerBase::Update();
  updated |= UpdateInternalContentScale();
  updated |= UpdateGeometry();
  updated |= SetHasFindInPageTickmarks(scrollbar_.Read(*this)->HasTickmarks());

  if (internal_content_bounds_.Read(*this).IsEmpty()) {
    if (track_and_buttons_resource_.Read(*this)) {
      track_and_buttons_resource_.Write(*this) = nullptr;
      thumb_resource_.Write(*this) = nullptr;
      SetNeedsPushProperties();
      updated = true;
    }
    return updated;
  }

  if (!has_thumb_.Read(*this) && thumb_resource_.Read(*this)) {
    thumb_resource_.Write(*this) = nullptr;
    SetNeedsPushProperties();
    updated = true;
  }

  updated |= UpdateTrackAndButtonsIfNeeded();
  updated |= UpdateThumbIfNeeded();

  return updated;
}

bool PaintedScrollbarLayer::UpdateTrackAndButtonsIfNeeded() {
  bool updated = false;
  gfx::Size size = bounds();
  gfx::Size scaled_size = internal_content_bounds_.Read(*this);
  if (!track_and_buttons_resource_.Read(*this) ||
      scrollbar_.Read(*this)->TrackAndButtonsNeedRepaint()) {
    if (uses_nine_patch_track_and_buttons_ &&
        // Can't use nine-patch track and buttons if tickmarks are present.
        !scrollbar_.Read(*this)->HasTickmarks()) {
      size = scrollbar_.Read(*this)->NinePatchTrackAndButtonsCanvasSize();
      scaled_size =
          gfx::ScaleToCeiledSize(size, internal_contents_scale_.Read(*this));
      track_and_buttons_aperture_.Write(*this) =
          scrollbar_.Read(*this)->NinePatchTrackAndButtonsAperture();
    }

    track_and_buttons_resource_.Write(*this) = ScopedUIResource::Create(
        layer_tree_host()->GetUIResourceManager(),
        RasterizeScrollbarPart(size, scaled_size,
                               [this, size](PaintCanvas& canvas) {
                                 scrollbar_.Write(*this)->PaintTrackAndButtons(
                                     canvas, gfx::Rect(size));
                               }));
    SetNeedsPushProperties();
    updated = true;
  }

  return updated;
}

bool PaintedScrollbarLayer::UpdateThumbIfNeeded() {
  bool updated = false;
  // If the scrollbar uses solid color thumb, it sends the correct color for
  // the thumb to the Impl class instead of generating a bitmap.
  if (uses_solid_color_thumb_) {
    if (scrollbar_.Read(*this)->ThumbNeedsRepaint() ||
        !thumb_color_.Read(*this).has_value()) {
      const SkColor4f thumb_color = scrollbar_.Read(*this)->ThumbColor();
      if (!thumb_color_.Read(*this).has_value() ||
          thumb_color != thumb_color_.Read(*this).value()) {
        thumb_color_.Write(*this) = thumb_color;
        SetNeedsPushProperties();
        updated = true;
      }
      // Clear thumb needs repaint regardless of if the thumb's color changed.
      scrollbar_.Write(*this)->ClearThumbNeedsRepaint();
    }
    return updated;
  }

  gfx::Size thumb_size = thumb_size_.Read(*this);
  gfx::Size scaled_thumb_size = LayerSizeToContentSize(thumb_size);
  if (has_thumb_.Read(*this) && !scaled_thumb_size.IsEmpty()) {
    if (!thumb_resource_.Read(*this) ||
        scrollbar_.Read(*this)->ThumbNeedsRepaint() ||
        scaled_thumb_size !=
            thumb_resource_.Write(*this)->GetBitmap(0, false).GetSize()) {
      thumb_resource_.Write(*this) = ScopedUIResource::Create(
          layer_tree_host()->GetUIResourceManager(),
          RasterizeScrollbarPart(thumb_size, scaled_thumb_size,
                                 [this, thumb_size](PaintCanvas& canvas) {
                                   scrollbar_.Write(*this)->PaintThumb(
                                       canvas, gfx::Rect(thumb_size));
                                 }));
      SetNeedsPushProperties();
      updated = true;
    }
    updated |= UpdateProperty(scrollbar_.Read(*this)->Opacity(),
                              &painted_opacity_.Write(*this));
  }

  return updated;
}

UIResourceBitmap PaintedScrollbarLayer::RasterizeScrollbarPart(
    const gfx::Size& size,
    const gfx::Size& requested_content_size,
    base::FunctionRef<void(PaintCanvas&)> paint_function) {
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
  canvas.clear(SkColors::kTransparent);

  float scale_x = content_size.width() / static_cast<float>(size.width());
  float scale_y = content_size.height() / static_cast<float>(size.height());
  canvas.scale(SkFloatToScalar(scale_x), SkFloatToScalar(scale_y));
  paint_function(canvas);
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
