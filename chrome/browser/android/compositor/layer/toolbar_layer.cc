// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/toolbar_layer.h"

#include "base/feature_list.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/slim/layer.h"
#include "cc/slim/nine_patch_layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/android/compositor/resources/toolbar_resource.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/offset_tag.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace {
// LINT.IfChange(InvalidContentOffset)
const float kInvalidContentOffset = -10001.f;
// LINT.ThenChange(//chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/top/TopToolbarOverlayMediator.java:InvalidContentOffset)
}  // namespace

namespace android {

// static
scoped_refptr<ToolbarLayer> ToolbarLayer::Create(
    ui::ResourceManager* resource_manager) {
  return base::WrapRefCounted(new ToolbarLayer(resource_manager));
}

scoped_refptr<cc::slim::Layer> ToolbarLayer::layer() {
  return layer_;
}

void ToolbarLayer::PushResource(int toolbar_resource_id,
                                int toolbar_background_color,
                                bool anonymize,
                                int toolbar_textbox_background_color,
                                int url_bar_background_resource_id,
                                float x_offset,
                                float y_offset,
                                float legacy_content_offset,
                                bool show_debug,
                                bool clip_shadow,
                                const viz::OffsetTag& offset_tag) {
  ToolbarResource* resource =
      ToolbarResource::From(resource_manager_->GetResource(
          ui::ANDROID_RESOURCE_TYPE_DYNAMIC, toolbar_resource_id));

  // TODO(https://crbug.com/466162772): after the progress bar is decoupled from
  // the toolbar, we can freely set the visibility of the toolbar without
  // worrying about the progress bar. If AnimatedProgressBar is enabled, ensure
  // that we don't hide the parent layer so that the progress bar is still
  // visible even when we don't have a capture for the toolbar.
  if (features::IsAndroidAnimatedProgressBarInBrowserEnabled()) {
    toolbar_background_layer_->SetHideLayerAndSubtree(!resource);
    url_bar_background_layer_->SetHideLayerAndSubtree(!resource);
    bitmap_layer_->SetHideLayerAndSubtree(!resource);
  } else {
    layer_->SetHideLayerAndSubtree(!resource);
  }
  if (!resource) {
    return;
  }

  // This layer effectively draws over the space the resource takes for shadows.
  // Set the bounds to the non-shadow size so that other things can properly
  // line up.
  gfx::Size toolbar_bounds =
      gfx::Size(resource->size().width(),
                resource->size().height() - resource->shadow_height());
  layer_->SetBounds(toolbar_bounds);

  toolbar_background_layer_->SetBounds(resource->toolbar_rect().size());
  toolbar_background_layer_->SetPosition(
      gfx::PointF(resource->toolbar_rect().origin()));
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  toolbar_background_layer_->SetBackgroundColor(
      SkColor4f::FromColor(toolbar_background_color));

  bool url_bar_visible = resource->location_bar_content_rect().width() != 0;
  url_bar_background_layer_->SetHideLayerAndSubtree(!url_bar_visible);
  if (url_bar_visible) {
    ui::NinePatchResource* url_bar_background_resource;
    url_bar_background_resource = ui::NinePatchResource::From(
        resource_manager_->GetStaticResourceWithTint(
            url_bar_background_resource_id, toolbar_textbox_background_color));

    gfx::Size draw_size(url_bar_background_resource->DrawSize(
        resource->location_bar_content_rect().size()));
    gfx::Rect border(url_bar_background_resource->Border(draw_size));
    gfx::PointF position(url_bar_background_resource->DrawPosition(
        resource->location_bar_content_rect().origin()));

    url_bar_background_layer_->SetBounds(draw_size);
    url_bar_background_layer_->SetPosition(position);
    url_bar_background_layer_->SetBorder(border);
    url_bar_background_layer_->SetAperture(
        url_bar_background_resource->aperture());
    url_bar_background_layer_->SetUIResourceId(
        url_bar_background_resource->ui_resource()->id());
  }

  bitmap_layer_->SetUIResourceId(resource->ui_resource()->id());
  bitmap_layer_->SetBounds(resource->size());

  layer_->SetMasksToBounds(clip_shadow);

  // The location bar background doubles as the anonymize layer -- it just
  // needs to be drawn on top of the toolbar bitmap.
  int background_layer_index = GetIndexOfLayer(toolbar_background_layer_);
  scoped_refptr<cc::slim::Layer> parent = ToolbarParentLayer();
  bool needs_move_to_front =
      anonymize && parent->children().back() != url_bar_background_layer_;
  bool needs_move_to_back =
      !anonymize &&
      parent->children()[background_layer_index] != url_bar_background_layer_;

  // If the layer needs to move, remove and re-add it.
  if (needs_move_to_front) {
    parent->AddChild(url_bar_background_layer_);
  } else if (needs_move_to_back) {
    parent->InsertChild(url_bar_background_layer_, background_layer_index + 1);
  }

  debug_layer_->SetBounds(resource->size());
  if (show_debug && !debug_layer_->parent())
    layer_->AddChild(debug_layer_);
  else if (!show_debug && debug_layer_->parent())
    debug_layer_->RemoveFromParent();

  // |legacy_content_offset| represents the bottom of the toolbar, assuming it's
  // always at the bottom of the browser controls. This is no longer the case
  // as for 2025.
  // TODO(https://crbug.com/454338286): Rename / remove in favor of y_Offset.
  if (!base::FeatureList::IsEnabled(chrome::android::kTopControlsRefactor) ||
      !base::FeatureList::IsEnabled(chrome::android::kTopControlsRefactorV2) ||
      kInvalidContentOffset != legacy_content_offset) {
    y_offset = legacy_content_offset - layer_->bounds().height();
  }

  layer_->SetPosition(gfx::PointF(x_offset, y_offset));

  if (features::IsAndroidAnimatedProgressBarInVizEnabled()) {
    toolbar_layers_->SetOffsetTag(offset_tag);
  } else {
    layer_->SetOffsetTag(offset_tag);
  }
}

int ToolbarLayer::GetIndexOfLayer(scoped_refptr<cc::slim::Layer> layer) {
  scoped_refptr<cc::slim::Layer> parent = ToolbarParentLayer();
  for (unsigned int i = 0; i < parent->children().size(); ++i) {
    if (parent->children()[i] == layer) {
      return i;
    }
  }

  return -1;
}

scoped_refptr<cc::slim::Layer> ToolbarLayer::ToolbarParentLayer() {
  if (features::IsAndroidAnimatedProgressBarInVizEnabled()) {
    return toolbar_layers_;
  } else {
    return layer_;
  }
}

void ToolbarLayer::UpdateProgressBar(int progress_bar_x,
                                     int progress_bar_y,
                                     int progress_bar_width,
                                     int progress_bar_height,
                                     int progress_bar_color,
                                     int progress_bar_background_x,
                                     int progress_bar_background_y,
                                     int progress_bar_background_width,
                                     int progress_bar_background_height,
                                     int progress_bar_background_color,
                                     int progress_bar_static_background_x,
                                     int progress_bar_static_background_width,
                                     int progress_bar_static_background_color,
                                     float corner_radius,
                                     bool progress_bar_visual_update_available,
                                     bool visible,
                                     const viz::OffsetTag& offset_tag) {
  bool is_progress_bar_visible = SkColorGetA(progress_bar_background_color);
  if (features::IsAndroidAnimatedProgressBarInVizEnabled() ||
      features::IsAndroidAnimatedProgressBarInBrowserEnabled()) {
    is_progress_bar_visible = visible;

    if (features::IsAndroidAnimatedProgressBarInVizEnabled()) {
      progress_bar_layers_->SetOffsetTag(offset_tag);
    }
  }

  progress_bar_background_layer_->SetHideLayerAndSubtree(!is_progress_bar_visible);
  progress_bar_layer_->SetHideLayerAndSubtree(!is_progress_bar_visible);
  progress_bar_static_background_layer_->SetHideLayerAndSubtree(
      !(is_progress_bar_visible && progress_bar_visual_update_available));

  if (is_progress_bar_visible) {
    if (features::IsAndroidAnimatedProgressBarInVizEnabled()) {
      // Use corner_radius for gap between the foreground and background layer.
      progress_bar_background_layer_->SetPosition(
          gfx::PointF(corner_radius * 2, progress_bar_background_y));
    } else {
      progress_bar_background_layer_->SetPosition(
          gfx::PointF(progress_bar_background_x, progress_bar_background_y));
    }
    progress_bar_background_layer_->SetBounds(
        gfx::Size(progress_bar_background_width,
                  progress_bar_background_height));
    // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
    progress_bar_background_layer_->SetBackgroundColor(
        SkColor4f::FromColor(progress_bar_background_color));
    progress_bar_background_layer_->SetRoundedCorner(gfx::RoundedCornersF(corner_radius));

    if (features::IsAndroidAnimatedProgressBarInVizEnabled()) {
      // Position the foregound layer to show 0% progress.
      progress_bar_layer_->SetPosition(
          gfx::PointF(-progress_bar_width, progress_bar_y));
    } else {
      progress_bar_layer_->SetPosition(
          gfx::PointF(progress_bar_x, progress_bar_y));
    }
    progress_bar_layer_->SetBounds(
        gfx::Size(progress_bar_width, progress_bar_height));
    // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
    progress_bar_layer_->SetBackgroundColor(
        SkColor4f::FromColor(progress_bar_color));
    progress_bar_layer_->SetRoundedCorner(gfx::RoundedCornersF(corner_radius));

    if (progress_bar_visual_update_available) {
      progress_bar_static_background_layer_->SetPosition(gfx::PointF(
          progress_bar_static_background_x, progress_bar_y));
      progress_bar_static_background_layer_->SetBounds(gfx::Size(
          progress_bar_static_background_width, progress_bar_height));
      progress_bar_static_background_layer_->SetBackgroundColor(
          SkColor4f::FromColor(progress_bar_static_background_color));
      progress_bar_static_background_layer_->SetRoundedCorner(gfx::RoundedCornersF(corner_radius));
    }
  }
}

void ToolbarLayer::SetOpacity(float opacity) {
  toolbar_background_layer_->SetOpacity(opacity);
  url_bar_background_layer_->SetOpacity(opacity);
  bitmap_layer_->SetOpacity(opacity);

  progress_bar_layer_->SetOpacity(opacity);
  progress_bar_background_layer_->SetOpacity(opacity);
  progress_bar_static_background_layer_->SetOpacity(opacity);
}

ToolbarLayer::ToolbarLayer(ui::ResourceManager* resource_manager)
    : resource_manager_(resource_manager),
      layer_(cc::slim::Layer::Create()),
      toolbar_layers_(cc::slim::Layer::Create()),
      progress_bar_layers_(cc::slim::Layer::Create()),
      toolbar_background_layer_(cc::slim::SolidColorLayer::Create()),
      url_bar_background_layer_(cc::slim::NinePatchLayer::Create()),
      bitmap_layer_(cc::slim::UIResourceLayer::Create()),
      progress_bar_layer_(cc::slim::SolidColorLayer::Create()),
      progress_bar_background_layer_(cc::slim::SolidColorLayer::Create()),
      progress_bar_static_background_layer_(
          cc::slim::SolidColorLayer::Create()),
      debug_layer_(cc::slim::SolidColorLayer::Create()) {
  if (features::IsAndroidAnimatedProgressBarInVizEnabled()) {
    // Parents are drawn before children. Children added first are drawn first.
    // Layers that are drawn later will cover all layers drawn before it.
    toolbar_background_layer_->SetIsDrawable(true);
    toolbar_layers_->AddChild(toolbar_background_layer_);

    url_bar_background_layer_->SetIsDrawable(true);
    url_bar_background_layer_->SetFillCenter(true);
    toolbar_layers_->AddChild(url_bar_background_layer_);

    bitmap_layer_->SetIsDrawable(true);
    toolbar_layers_->AddChild(bitmap_layer_);

    progress_bar_static_background_layer_->SetIsDrawable(true);
    progress_bar_static_background_layer_->SetHideLayerAndSubtree(true);
    toolbar_layers_->AddChild(progress_bar_static_background_layer_);

    layer_->AddChild(toolbar_layers_);

    progress_bar_layer_->SetIsDrawable(true);
    progress_bar_layer_->SetHideLayerAndSubtree(true);
    progress_bar_layers_->AddChild(progress_bar_layer_);

    progress_bar_background_layer_->SetIsDrawable(true);
    progress_bar_background_layer_->SetHideLayerAndSubtree(true);
    progress_bar_layers_->AddChild(progress_bar_background_layer_);

    layer_->AddChild(progress_bar_layers_);
  } else {
    toolbar_background_layer_->SetIsDrawable(true);
    layer_->AddChild(toolbar_background_layer_);

    url_bar_background_layer_->SetIsDrawable(true);
    url_bar_background_layer_->SetFillCenter(true);
    layer_->AddChild(url_bar_background_layer_);

    bitmap_layer_->SetIsDrawable(true);
    layer_->AddChild(bitmap_layer_);

    progress_bar_static_background_layer_->SetIsDrawable(true);
    progress_bar_static_background_layer_->SetHideLayerAndSubtree(true);
    layer_->AddChild(progress_bar_static_background_layer_);

    progress_bar_background_layer_->SetIsDrawable(true);
    progress_bar_background_layer_->SetHideLayerAndSubtree(true);
    layer_->AddChild(progress_bar_background_layer_);

    progress_bar_layer_->SetIsDrawable(true);
    progress_bar_layer_->SetHideLayerAndSubtree(true);
    layer_->AddChild(progress_bar_layer_);
  }

  debug_layer_->SetIsDrawable(true);
  debug_layer_->SetBackgroundColor(SkColors::kGreen);
  debug_layer_->SetOpacity(0.5f);
}

ToolbarLayer::~ToolbarLayer() = default;

}  //  namespace android
