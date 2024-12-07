// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/decoration_tab_title.h"

#include <android/bitmap.h>

#include "chrome/browser/android/compositor/decoration_icon_title.h"
#include "components/viz/common/features.h"
#include "ui/android/resources/resource_manager.h"

namespace android {

DecorationTabTitle::DecorationTabTitle(ui::ResourceManager* resource_manager,
                                       int title_resource_id,
                                       int icon_resource_id,
                                       int spinner_resource_id,
                                       int spinner_incognito_resource_id,
                                       int fade_width,
                                       int icon_start_padding,
                                       int icon_end_padding,
                                       bool is_incognito,
                                       bool is_rtl)
    : DecorationIconTitle(resource_manager,
                          title_resource_id,
                          icon_resource_id,
                          fade_width,
                          icon_start_padding,
                          icon_end_padding,
                          is_incognito,
                          is_rtl),
      spinner_resource_id_(spinner_resource_id),
      spinner_incognito_resource_id_(spinner_incognito_resource_id) {}

DecorationTabTitle::~DecorationTabTitle() = default;

void DecorationTabTitle::SetUIResourceIds() {
  DecorationTitle::SetUIResourceIds();

  if (!is_loading_) {
    handleIconResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP);
  } else if (spinner_resource_id_ != 0 && spinner_incognito_resource_id_ != 0) {
    int resource_id =
        is_incognito_ ? spinner_incognito_resource_id_ : spinner_resource_id_;

    ui::Resource* spinner_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_STATIC, resource_id);

    if (spinner_resource) {
      layer_icon_->SetUIResourceId(spinner_resource->ui_resource()->id());
    }

    // Rotate about the center of the layer.
    layer_icon_->SetTransformOrigin(
        gfx::PointF(icon_size_.width() / 2, icon_size_.height() / 2));
  }
  size_ = DecorationIconTitle::calculateSize(icon_size_.width());
}

void DecorationTabTitle::SetIsLoading(bool is_loading) {
  if (is_loading != is_loading_) {
    is_loading_ = is_loading;
    SetUIResourceIds();
  }
}

void DecorationTabTitle::SetSpinnerRotation(float rotation) {
  if (!is_loading_) {
    return;
  }
  float diff = rotation - spinner_rotation_;
  spinner_rotation_ = rotation;
  if (diff != 0) {
    transform_->RotateAboutZAxis(diff);
  }
  layer_icon_->SetTransform(*transform_.get());
}

}  // namespace android
