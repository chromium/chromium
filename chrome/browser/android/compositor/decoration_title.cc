// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/decoration_title.h"

#include <android/bitmap.h>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/slim/layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "components/viz/common/features.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace android {

DecorationTitle::DecorationTitle(ui::ResourceManager* resource_manager,
                                 int title_resource_id,
                                 int favicon_resource_id,
                                 int spinner_resource_id,
                                 int spinner_incognito_resource_id,
                                 int fade_width,
                                 int favicon_start_padding,
                                 int favicon_end_padding,
                                 bool is_incognito,
                                 bool is_rtl)
    : layer_(cc::slim::Layer::Create()),
      layer_opaque_(cc::slim::UIResourceLayer::Create()),
      layer_fade_(cc::slim::UIResourceLayer::Create()),
      layer_favicon_(cc::slim::UIResourceLayer::Create()),
      title_resource_id_(title_resource_id),
      favicon_resource_id_(favicon_resource_id),
      spinner_resource_id_(spinner_resource_id),
      spinner_incognito_resource_id_(spinner_incognito_resource_id),
      fade_width_(fade_width),
      spinner_rotation_(0),
      favicon_start_padding_(favicon_start_padding),
      favicon_end_padding_(favicon_end_padding),
      is_incognito_(is_incognito),
      is_rtl_(is_rtl),
      is_loading_(false),
      transform_(new gfx::Transform()),
      resource_manager_(resource_manager) {
  layer_->AddChild(layer_favicon_);
  layer_->AddChild(layer_opaque_);
  layer_->AddChild(layer_fade_);
}

DecorationTitle::~DecorationTitle() {
  layer_->RemoveFromParent();
}

void DecorationTitle::SetResourceManager(
    ui::ResourceManager* resource_manager) {
  resource_manager_ = resource_manager;
}

void DecorationTitle::Update(int title_resource_id,
                             int favicon_resource_id,
                             int fade_width,
                             int favicon_start_padding,
                             int favicon_end_padding,
                             bool is_incognito,
                             bool is_rtl) {
  title_resource_id_ = title_resource_id;
  favicon_resource_id_ = favicon_resource_id;
  is_incognito_ = is_incognito;
  is_rtl_ = is_rtl;
  favicon_start_padding_ = favicon_start_padding;
  favicon_end_padding_ = favicon_end_padding;
  fade_width_ = fade_width;
}

void DecorationTitle::SetFaviconResourceId(int favicon_resource_id) {
  favicon_resource_id_ = favicon_resource_id;
}

void DecorationTitle::SetUIResourceIds() {
  ui::Resource* title_resource = resource_manager_->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP, title_resource_id_);
  if (title_resource) {
    layer_opaque_->SetUIResourceId(title_resource->ui_resource()->id());
    layer_fade_->SetUIResourceId(title_resource->ui_resource()->id());
    title_size_ = title_resource->size();
  }

  if (!is_loading_ && favicon_resource_id_ != -1) {
    ui::Resource* favicon_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP, favicon_resource_id_);
    if (favicon_resource) {
      layer_favicon_->SetUIResourceId(favicon_resource->ui_resource()->id());
      favicon_size_ = favicon_resource->size();
    } else {
      layer_favicon_->SetUIResourceId(0);
    }
    layer_favicon_->SetTransform(gfx::Transform());
  } else if (spinner_resource_id_ != -1 &&
             spinner_incognito_resource_id_ != -1) {
    int resource_id =
        is_incognito_ ? spinner_incognito_resource_id_ : spinner_resource_id_;

    ui::Resource* spinner_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_STATIC, resource_id);

    if (spinner_resource)
      layer_favicon_->SetUIResourceId(spinner_resource->ui_resource()->id());

    // Rotate about the center of the layer.
    layer_favicon_->SetTransformOrigin(
        gfx::PointF(favicon_size_.width() / 2, favicon_size_.height() / 2));
  }

  size_ = gfx::Size(title_size_.width() + favicon_size_.width(),
                    title_size_.height());
}

void DecorationTitle::SetIsLoading(bool is_loading) {
  if (is_loading != is_loading_) {
    is_loading_ = is_loading;
    SetUIResourceIds();
  }
}

void DecorationTitle::SetSpinnerRotation(float rotation) {
  if (!is_loading_)
    return;
  float diff = rotation - spinner_rotation_;
  spinner_rotation_ = rotation;
  if (diff != 0) {
    transform_->RotateAboutZAxis(diff);
  }
  layer_favicon_->SetTransform(*transform_.get());
}

void DecorationTitle::setOpacity(float opacity) {
  layer_opaque_->SetOpacity(opacity);
  layer_favicon_->SetOpacity(opacity);
  layer_fade_->SetOpacity(opacity);
}

void DecorationTitle::setBounds(const gfx::Size& bounds) {
  layer_->SetBounds(gfx::Size(bounds.width(), size_.height()));

  if (bounds.GetArea() == 0.f) {
    layer_->SetHideLayerAndSubtree(true);
    return;
  }
  layer_->SetHideLayerAndSubtree(false);

  // Current implementation assumes there is always enough space
  // to draw favicon and title fade.

  // Note that favicon positon and title aligning depends on the system locale,
  // l10n_util::IsLayoutRtl(), while title starting and fade out direction
  // depends on the title text locale (DecorationTitle::is_rtl_).

  bool sys_rtl = l10n_util::IsLayoutRtl();
  int favicon_space =
      favicon_size_.width() + favicon_start_padding_ + favicon_end_padding_;
  int title_space = std::max(0, bounds.width() - favicon_space - fade_width_);
  int fade_space = std::max(0, bounds.width() - favicon_space - title_space);

  if (title_size_.width() <= title_space + fade_space)
    title_space += fade_space;

  if (title_size_.width() <= title_space)
    fade_space = 0.f;

  float favicon_offset_y = (size_.height() - favicon_size_.height()) / 2.f;
  float title_offset_y = (size_.height() - title_size_.height()) / 2.f;

  // Step 1. Place favicon.
  if (favicon_resource_id_ != -1) {
    int favicon_x = favicon_start_padding_;
    if (sys_rtl) {
      favicon_x =
          bounds.width() - favicon_size_.width() - favicon_start_padding_;
    }
    layer_favicon_->SetIsDrawable(true);
    layer_favicon_->SetBounds(favicon_size_);
    layer_favicon_->SetPosition(gfx::PointF(favicon_x, favicon_offset_y));
  }

  // Step 2. Place the opaque title component.
  if (title_space > 0.f) {
    // Calculate the title position and size, taking into account both
    // system and title RTL.
    int width = std::min(title_space, title_size_.width());
    int x_offset = sys_rtl ? title_space - width : favicon_space;
    int x = x_offset + (is_rtl_ ? fade_space : 0);

    // Calculate the UV coordinates taking into account title RTL.
    float width_percentage = (float)width / title_size_.width();
    float u1 = is_rtl_ ? 1.f - width_percentage : 0.f;
    float u2 = is_rtl_ ? 1.f : width_percentage;

    layer_opaque_->SetIsDrawable(true);
    layer_opaque_->SetBounds(gfx::Size(width, title_size_.height()));
    layer_opaque_->SetPosition(gfx::PointF(x, title_offset_y));
    layer_opaque_->SetUV(gfx::PointF(u1, 0.f), gfx::PointF(u2, 1.f));
  } else {
    layer_opaque_->SetIsDrawable(false);
  }

  // Step 3. Place the fade title component.
  if (fade_space > 0.f) {
    // Calculate the title position and size, taking into account both
    // system and title RTL.
    int x_offset = sys_rtl ? 0 : favicon_space;
    int x = x_offset + (is_rtl_ ? 0 : title_space);
    float title_amt = (float)title_space / title_size_.width();
    float fade_amt = (float)fade_space / title_size_.width();

    // Calculate UV coordinates taking into account title RTL.
    float u1 = is_rtl_ ? 1.f - title_amt - fade_amt : title_amt;
    float u2 = is_rtl_ ? 1.f - title_amt : title_amt + fade_amt;

    // Calculate vertex alpha taking into account title RTL.
    float max_alpha = (float)fade_space / fade_width_;
    float a1 = is_rtl_ ? 0.f : max_alpha;
    float a2 = is_rtl_ ? max_alpha : 0.f;

    layer_fade_->SetIsDrawable(true);
    layer_fade_->SetBounds(gfx::Size(fade_space, title_size_.height()));
    layer_fade_->SetPosition(gfx::PointF(x, title_offset_y));
    layer_fade_->SetUV(gfx::PointF(u1, 0.f), gfx::PointF(u2, 1.f));
    // Left to right gradient.
    gfx::LinearGradient gradient;
    gradient.AddStep(0.f, a1 * 255);
    gradient.AddStep(1.f, a2 * 255);
    gradient.set_angle(0);

    // TODO(b/361804880) With BCIV, offsets aren't applied correctly to layers
    // with clipped regions. Remove the fade effect from the title for now, so
    // we can still run the experiment for large screens.
    if (!features::IsBrowserControlsInVizEnabled()) {
      layer_fade_->SetGradientMask(gradient);
    }
  } else {
    layer_fade_->SetIsDrawable(false);
  }
}

scoped_refptr<cc::slim::Layer> DecorationTitle::layer() {
  DCHECK(layer_.get());
  return layer_;
}

}  // namespace android
