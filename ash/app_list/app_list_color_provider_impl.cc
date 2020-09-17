// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_color_provider_impl.h"

#include "ash/style/ash_color_provider.h"

namespace ash {

AppListColorProviderImpl::AppListColorProviderImpl()
    : ash_color_provider_(AshColorProvider::Get()) {}

AppListColorProviderImpl::~AppListColorProviderImpl() = default;

SkColor AppListColorProviderImpl::GetExpandArrowInkDropBaseColor() const {
  return ash_color_provider_
      ->GetRippleAttributes(GetExpandArrowIconBackgroundColor())
      .base_color;
}

SkColor AppListColorProviderImpl::GetExpandArrowIconBaseColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
}

SkColor AppListColorProviderImpl::GetExpandArrowIconBackgroundColor() const {
  return ash_color_provider_->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

SkColor AppListColorProviderImpl::GetAppListBackgroundColor() const {
  return ash_color_provider_->GetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield80);
}

SkColor AppListColorProviderImpl::GetSearchBoxBackgroundColor() const {
  return ash_color_provider_->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

SkColor AppListColorProviderImpl::GetSearchBoxPlaceholderTextColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary);
}

SkColor AppListColorProviderImpl::GetSearchBoxTextColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
}

SkColor AppListColorProviderImpl::GetSuggestionChipBackgroundColor() const {
  return ash_color_provider_->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

SkColor AppListColorProviderImpl::GetSuggestionChipTextColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
}

SkColor AppListColorProviderImpl::GetAppListItemTextColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
}

}  // namespace ash
