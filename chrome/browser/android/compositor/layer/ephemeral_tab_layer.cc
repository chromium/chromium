// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/ephemeral_tab_layer.h"

#include "base/task/cancelable_task_tracker.h"
#include "cc/layers/layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace {

void DisplayFavicon(scoped_refptr<cc::UIResourceLayer> layer,
                    const SkBitmap& favicon,
                    const float dp_to_px,
                    const float panel_width,
                    const float bar_height,
                    const float bar_margin,
                    const base::RepeatingCallback<void()>& favicon_callback) {
  const float bounds_width =
      android::EphemeralTabLayer::kFaviconWidthDp * dp_to_px;

  // Dimension to which favicons are resized - half the size of default icon.
  const float icon_size = bounds_width / 2.0f;
  const float padding = bar_margin + (bounds_width - icon_size) / 2.0f;
  layer->SetBitmap(favicon);
  layer->SetBounds(gfx::Size(icon_size, icon_size));

  bool is_rtl = l10n_util::IsLayoutRtl();
  float icon_x = is_rtl ? panel_width - icon_size - padding : padding;
  float icon_y = (bar_height - icon_size) / 2;
  layer->SetPosition(gfx::PointF(icon_x, icon_y));
  favicon_callback.Run();
}

}  // namespace

namespace android {

// static
scoped_refptr<EphemeralTabLayer> EphemeralTabLayer::Create(
    ui::ResourceManager* resource_manager,
    base::RepeatingCallback<void()>&& favicon_callback) {
  return base::WrapRefCounted(
      new EphemeralTabLayer(resource_manager, std::move(favicon_callback)));
}

void EphemeralTabLayer::SetProperties(
    content::WebContents* web_contents,
    int title_view_resource_id,
    int caption_view_resource_id,
    int security_icon_resource_id,
    jfloat security_icon_opacity,
    jfloat caption_animation_percentage,
    jfloat text_layer_min_height,
    jfloat title_caption_spacing,
    jboolean caption_visible,
    int progress_bar_background_resource_id,
    int progress_bar_resource_id,
    float dp_to_px,
    const scoped_refptr<cc::Layer>& content_layer,
    float panel_x,
    float panel_y,
    float panel_width,
    float panel_height,
    int bar_background_color,
    float bar_margin_side,
    float bar_margin_top,
    float bar_height,
    bool bar_border_visible,
    float bar_border_height,
    int icon_color,
    int drag_handlebar_color,
    jfloat favicon_opacity,
    bool progress_bar_visible,
    float progress_bar_height,
    float progress_bar_opacity,
    float progress_bar_completion,
    int separator_line_color,
    bool is_new_layout) {
  if (web_contents_ != web_contents) {
    web_contents_ = web_contents;
    if (web_contents_) {
      auto* favicon_driver =
          favicon::ContentFaviconDriver::FromWebContents(web_contents_);
      if (favicon_driver)
        favicon_driver->AddObserver(this);
      // No need to remove the observer from the previous WebContents since
      // it is already destroyed by the time it reaches this point.
    }
  }

  // Round values to avoid pixel gap between layers.
  bar_height = floor(bar_height);
  float bar_top = 0.f;
  float bar_bottom = bar_top + bar_height;

  float title_opacity = 0.f;
  OverlayPanelLayer::SetProperties(
      dp_to_px, content_layer, bar_height, panel_x, panel_y, panel_width,
      panel_height, bar_background_color, bar_margin_side, bar_margin_top,
      bar_height, 0.0f, title_opacity, bar_border_visible, bar_border_height,
      icon_color, drag_handlebar_color, 1.0f /* icon opacity */,
      separator_line_color);

  // Content setup, to center in space below drag handle (when present).
  int content_top = bar_top;
  int content_height = bar_height;
  if (is_new_layout) {
    content_top += bar_margin_top;
    content_height -= bar_margin_top;
  }
  SetupTextLayer(content_top, content_height, text_layer_min_height,
                 caption_view_resource_id, security_icon_resource_id,
                 security_icon_opacity, caption_animation_percentage,
                 caption_visible, title_view_resource_id,
                 title_caption_spacing);

  OverlayPanelLayer::SetProgressBar(
      progress_bar_background_resource_id, progress_bar_resource_id,
      progress_bar_visible, bar_bottom, progress_bar_height,
      progress_bar_opacity, progress_bar_completion, panel_width);
  dp_to_px_ = dp_to_px;
  panel_width_ = panel_width;
  bar_height_ = bar_height;
  bar_margin_side_ = bar_margin_side;
  if (favicon_opacity > 0.f)
    favicon_layer_->SetIsDrawable(true);
  favicon_layer_->SetOpacity(favicon_opacity);
  panel_icon_->SetOpacity(1 - favicon_opacity);
}

void EphemeralTabLayer::SetupTextLayer(float content_top,
                                       float content_height,
                                       float text_layer_min_height,
                                       int caption_view_resource_id,
                                       int security_icon_resource_id,
                                       float security_icon_opacity,
                                       float animation_percentage,
                                       bool caption_visible,
                                       int title_resource_id,
                                       float title_caption_spacing) {
  // ---------------------------------------------------------------------------
  // Setup the Drawing Hierarchy
  // ---------------------------------------------------------------------------

  DCHECK(text_layer_.get());
  DCHECK(caption_.get());
  DCHECK(title_.get());

  // Title
  ui::Resource* title_resource = resource_manager_->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, title_resource_id);
  if (title_resource) {
    title_->SetUIResourceId(title_resource->ui_resource()->id());
    title_->SetBounds(title_resource->size());
  }

  // Caption
  ui::Resource* caption_resource = nullptr;
  if (caption_visible) {
    // Grabs the dynamic Search Caption resource so we can get a snapshot.
    caption_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_DYNAMIC, caption_view_resource_id);
  }

  if (animation_percentage != 0.f) {
    if (caption_->parent() != text_layer_)
      text_layer_->AddChild(caption_);

    if (security_icon_layer_->parent() != text_layer_)
      text_layer_->AddChild(security_icon_layer_);
    if (caption_resource) {
      caption_->SetUIResourceId(caption_resource->ui_resource()->id());
      caption_->SetBounds(caption_resource->size());
    }
  } else {
    if (caption_->parent())
      caption_->RemoveFromParent();
    if (security_icon_layer_->parent())
      security_icon_layer_->RemoveFromParent();
  }

  // Set up security icon layer
  if (security_icon_resource_id) {
    ui::Resource* security_icon_resource =
        resource_manager_->GetStaticResourceWithTint(security_icon_resource_id,
                                                     0);
    security_icon_layer_->SetUIResourceId(
        security_icon_resource->ui_resource()->id());
    security_icon_layer_->SetBounds(gfx::ScaleToRoundedSize(
        security_icon_resource->size(), kSecurityIconScale));
  }

  // ---------------------------------------------------------------------------
  // Calculate Text Layer Size
  // ---------------------------------------------------------------------------
  float title_height = title_->bounds().height();
  float caption_height = caption_visible ? caption_->bounds().height() : 0.f;

  float layer_height =
      std::max(text_layer_min_height,
               title_height + caption_height + title_caption_spacing);
  float layer_width =
      std::max(title_->bounds().width(), caption_->bounds().width());

  float layer_top = content_top + (content_height - layer_height) / 2;
  text_layer_->SetBounds(gfx::Size(layer_width, layer_height));
  text_layer_->SetPosition(gfx::PointF(0.f, layer_top));
  text_layer_->SetMasksToBounds(true);

  // ---------------------------------------------------------------------------
  // Layout Text Layer
  // ---------------------------------------------------------------------------
  // ---Top of Panel Bar---  <- bar_top
  //
  // ---Top of Text Layer--- <- layer_top
  //                         } remaining_height / 2
  //      Title              } title_height
  // --Bottom of Text Layer-
  //
  // --Bottom of Panel Bar-

  float title_top = (layer_height - title_height) / 2;

  // If we aren't displaying the caption we're done.
  if (animation_percentage == 0.f || !caption_resource) {
    title_->SetPosition(gfx::PointF(0.f, title_top));
    return;
  }

  // Calculate the positions for the Title and Caption when the Caption
  // animation is complete.
  float remaining_height =
      layer_height - title_height - title_caption_spacing - caption_height;

  float title_top_end = remaining_height / 2;
  float caption_top_end = title_top_end + title_height + title_caption_spacing;

  // Interpolate between the animation start and end positions (short cut
  // if the animation is at the end or start).
  title_top = title_top * (1.f - animation_percentage) +
              title_top_end * animation_percentage;

  // The Caption starts off the bottom of the Text Layer.
  float caption_top = layer_height * (1.f - animation_percentage) +
                      caption_top_end * animation_percentage;

  title_->SetPosition(gfx::PointF(0.f, title_top));
  bool is_rtl = l10n_util::IsLayoutRtl();
  float caption_left = 0.f;
  if (is_rtl && security_icon_resource_id) {
    caption_left = security_icon_layer_->bounds().width();
  }
  caption_->SetPosition(gfx::PointF(caption_left, caption_top));

  // Security icon
  if (!security_icon_resource_id)
    return;

  float icon_x;
  if (is_rtl) {
    icon_x = bar_margin_side_ * 2 + close_icon_->bounds().width();
    if (open_tab_icon_resource_id_ != kInvalidResourceID) {
      icon_x += bar_margin_side_ * 2 + open_tab_icon_->bounds().width();
    }
  } else {
    icon_x = bar_margin_side_ +
             (kFaviconWidthDp + kSecurityIconMarginStartDp) * dp_to_px_;
  }
  float icon_y = caption_top + (caption_->bounds().height() -
                                security_icon_layer_->bounds().height()) /
                                   2;
  security_icon_layer_->SetPosition(gfx::PointF(icon_x, icon_y));
  security_icon_layer_->SetOpacity(security_icon_opacity);
}

void EphemeralTabLayer::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  SkBitmap favicon_bitmap =
      image.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
  if (favicon_bitmap.empty())
    return;
  std::string host = icon_url.host();
  if (host == favicon_url_host_)
    return;
  favicon_url_host_ = host;
  favicon_bitmap.setImmutable();
  DisplayFavicon(favicon_layer_, favicon_bitmap, dp_to_px_, panel_width_,
                 bar_height_, bar_margin_side_, favicon_callback_);
}

void EphemeralTabLayer::OnHide() {
  favicon_layer_->SetIsDrawable(false);
  favicon_url_host_.clear();
  web_contents_ = nullptr;
}

EphemeralTabLayer::EphemeralTabLayer(
    ui::ResourceManager* resource_manager,
    base::RepeatingCallback<void()>&& favicon_callback)
    : OverlayPanelLayer(resource_manager),
      favicon_callback_(std::move(favicon_callback)),
      title_(cc::UIResourceLayer::Create()),
      caption_(cc::UIResourceLayer::Create()),
      favicon_layer_(cc::UIResourceLayer::Create()),
      security_icon_layer_(cc::UIResourceLayer::Create()),
      text_layer_(cc::UIResourceLayer::Create()) {
  // Content layer
  text_layer_->SetIsDrawable(true);
  title_->SetIsDrawable(true);
  caption_->SetIsDrawable(true);
  security_icon_layer_->SetIsDrawable(true);

  AddBarTextLayer(text_layer_);
  text_layer_->AddChild(title_);

  favicon_layer_->SetIsDrawable(true);
  layer_->AddChild(favicon_layer_);
  cancelable_task_tracker_.reset(new base::CancelableTaskTracker());
}

EphemeralTabLayer::~EphemeralTabLayer() {}

}  //  namespace android
