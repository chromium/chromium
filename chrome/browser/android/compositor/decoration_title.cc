// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/decoration_title.h"

#include <android/bitmap.h>

// #include "base/i18n/rtl.h"
#include "base/feature_list.h"
#include "cc/slim/layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"

namespace android {

DecorationTitle::DecorationTitle(ui::ResourceManager* resource_manager,
                                 int title_resource_id,
                                 int fade_width,
                                 bool is_incognito,
                                 bool is_rtl)
    : layer_(cc::slim::Layer::Create()),
      layer_opaque_(cc::slim::UIResourceLayer::Create()),
      layer_fade_(cc::slim::UIResourceLayer::Create()),
      title_resource_id_(title_resource_id),
      fade_width_(fade_width),
      is_incognito_(is_incognito),
      is_rtl_(is_rtl),
      resource_manager_(resource_manager) {
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
                             int fade_width,
                             bool is_incognito,
                             bool is_rtl) {
  title_resource_id_ = title_resource_id;
  is_incognito_ = is_incognito;
  is_rtl_ = is_rtl;
  fade_width_ = fade_width;
  needs_refresh_ = true;
}

void DecorationTitle::SetUIResourceIds() {
  if (!needs_refresh_ && base::FeatureList::IsEnabled(
                             chrome::android::kReloadTabUiResourcesIfChanged)) {
    return;
  }
  ui::Resource* title_resource = resource_manager_->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP, title_resource_id_);
  if (title_resource) {
    layer_opaque_->SetUIResourceId(title_resource->ui_resource()->id());
    layer_fade_->SetUIResourceId(title_resource->ui_resource()->id());
    title_size_ = title_resource->size();
  }
  size_ = calculateSize(0);
  needs_refresh_ = false;
}

gfx::Size DecorationTitle::calculateSize(int favicon_width) {
  return gfx::Size(title_size_.width() + favicon_width, title_size_.height());
}

void DecorationTitle::setOpacity(float opacity) {
  layer_opaque_->SetOpacity(opacity);
  layer_fade_->SetOpacity(opacity);
}

void DecorationTitle::setBounds(const gfx::Size& bounds) {
  setBounds(bounds, 0);
}

void DecorationTitle::setBounds(const gfx::Size& bounds, int start_space) {
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
  int title_space = std::max(0, bounds.width() - start_space - fade_width_);
  int fade_space = std::max(0, bounds.width() - start_space - title_space);

  if (title_size_.width() <= title_space + fade_space)
    title_space += fade_space;

  if (title_size_.width() <= title_space)
    fade_space = 0.f;

  float title_offset_y = (size_.height() - title_size_.height()) / 2.f;

  //  Place the opaque title component.
  if (title_space > 0.f) {
    // Calculate the title position and size, taking into account both
    // system and title RTL.
    int width = std::min(title_space, title_size_.width());
    int x_offset = sys_rtl ? title_space - width : start_space;
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

  // Place the fade title component.
  if (fade_space > 0.f) {
    // Calculate the title position and size, taking into account both
    // system and title RTL.
    int x_offset = sys_rtl ? 0 : start_space;
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
    layer_fade_->SetGradientMask(gradient);
  } else {
    layer_fade_->SetIsDrawable(false);
  }
}

scoped_refptr<cc::slim::Layer> DecorationTitle::layer() {
  DCHECK(layer_.get());
  return layer_;
}

}  // namespace android
