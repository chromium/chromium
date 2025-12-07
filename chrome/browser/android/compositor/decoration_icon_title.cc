// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/decoration_icon_title.h"

#include <android/bitmap.h>

#include "base/feature_list.h"
#include "cc/slim/layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"

namespace android {

DecorationIconTitle::DecorationIconTitle(ui::ResourceManager* resource_manager,
                                         int title_resource_id,
                                         int icon_resource_id,
                                         int fade_width,
                                         int icon_start_padding,
                                         int icon_end_padding,
                                         bool is_incognito,
                                         bool is_rtl)
    : DecorationTitle(resource_manager,
                      title_resource_id,
                      fade_width,
                      is_incognito,
                      is_rtl),
      layer_icon_(cc::slim::UIResourceLayer::Create()),
      icon_resource_id_(icon_resource_id),
      icon_start_padding_(icon_start_padding),
      icon_end_padding_(icon_end_padding),
      transform_(new gfx::Transform()) {
  layer_->AddChild(layer_icon_);
}

DecorationIconTitle::~DecorationIconTitle() = default;

void DecorationIconTitle::Update(int title_resource_id,
                                 int icon_resource_id,
                                 int fade_width,
                                 int icon_start_padding,
                                 int icon_end_padding,
                                 bool is_incognito,
                                 bool is_rtl) {
  DecorationTitle::Update(title_resource_id, fade_width, is_incognito, is_rtl);
  icon_resource_id_ = icon_resource_id;
  icon_start_padding_ = icon_start_padding;
  icon_end_padding_ = icon_end_padding;
  icon_needs_refresh_ = true;
}

void DecorationIconTitle::SetIconResourceId(int icon_resource_id) {
  icon_resource_id_ = icon_resource_id;
  icon_needs_refresh_ = true;
}

void DecorationIconTitle::SetUIResourceIds() {
  DecorationTitle::SetUIResourceIds();
  handleIconResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC);
  calculateSize(icon_size_.width());
}

gfx::Size DecorationIconTitle::calculateSize(int icon_width) {
  size_ = DecorationTitle::calculateSize(icon_width);
  return size_;
}

void DecorationIconTitle::handleIconResource(
    ui::AndroidResourceType resource_type) {
  if (!icon_needs_refresh_ &&
      base::FeatureList::IsEnabled(
          chrome::android::kReloadTabUiResourcesIfChanged)) {
    return;
  }
  if (icon_resource_id_ != ui::Resource::kInvalidResourceId) {
    ui::Resource* icon_resource =
        resource_manager_->GetResource(resource_type, icon_resource_id_);
    if (icon_resource) {
      layer_icon_->SetUIResourceId(icon_resource->ui_resource()->id());
      icon_size_ = icon_resource->size();
    } else {
      layer_icon_->SetUIResourceId(ui::Resource::kInvalidResourceId);
    }
    layer_icon_->SetTransform(gfx::Transform());
  } else {
    layer_icon_->SetUIResourceId(ui::Resource::kInvalidResourceId);
    icon_size_ = gfx::Size(0, 0);
  }
  icon_needs_refresh_ = false;
}

void DecorationIconTitle::setOpacity(float opacity) {
  DecorationTitle::setOpacity(opacity);
  layer_icon_->SetOpacity(opacity);
}

void DecorationIconTitle::SetShouldHideTitleText(bool should_hide_title_text) {
  DecorationTitle::SetShouldHideTitleText(should_hide_title_text);
}

void DecorationIconTitle::SetShouldHideIcon(bool should_hide_icon) {
  should_hide_icon_ = should_hide_icon;
}

void DecorationIconTitle::setBounds(const gfx::Size& bounds) {
  // Place icon.
  int icon_space =
      should_hide_icon_
          ? 0.f
          : icon_size_.width() + icon_start_padding_ + icon_end_padding_;
  float icon_offset_y = (size_.height() - icon_size_.height()) / 2.f;
  bool sys_rtl = l10n_util::IsLayoutRtl();

  if (should_hide_icon_) {
    layer_icon_->SetIsDrawable(false);
  } else if (icon_resource_id_ != ui::Resource::kInvalidResourceId) {
    int icon_x = icon_start_padding_;
    if (sys_rtl) {
      icon_x = bounds.width() - icon_size_.width() - icon_start_padding_;
    }
    layer_icon_->SetIsDrawable(true);
    layer_icon_->SetBounds(icon_size_);
    icon_position_ = gfx::PointF(icon_x, icon_offset_y);
    layer_icon_->SetPosition(icon_position_);
  }

  // Place opaque and fade title component.
  DecorationTitle::setBounds(bounds, icon_space);
}

}  // namespace android
