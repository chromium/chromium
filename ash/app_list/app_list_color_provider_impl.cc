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

SkColor AppListColorProviderImpl::GetSearchBoxCardBackgroundColor() const {
  // Set solid color background to avoid broken text. See crbug.com/746563.
  return ash_color_provider_->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kOpaque);
}

SkColor AppListColorProviderImpl::GetSearchBoxPlaceholderTextColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary);
}

SkColor AppListColorProviderImpl::GetSearchBoxTextColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
}

SkColor AppListColorProviderImpl::GetSearchBoxSecondaryTextColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary);
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

SkColor AppListColorProviderImpl::GetPageSwitcherButtonColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
}

SkColor AppListColorProviderImpl::GetPageSwitcherInkDropBaseColor() const {
  AshColorProvider::RippleAttributes ripple_attributes =
      ash_color_provider_->GetRippleAttributes(GetAppListBackgroundColor());
  return SkColorSetA(ripple_attributes.base_color,
                     ripple_attributes.inkdrop_opacity * 255);
}

SkColor AppListColorProviderImpl::GetPageSwitcherInkDropHighlightColor() const {
  AshColorProvider::RippleAttributes ripple_attributes =
      ash_color_provider_->GetRippleAttributes(GetAppListBackgroundColor());
  return SkColorSetA(ripple_attributes.base_color,
                     ripple_attributes.highlight_opacity * 255);
}

SkColor AppListColorProviderImpl::GetSearchBoxIconColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
}

SkColor AppListColorProviderImpl::GetFolderBackgroundColor() const {
  return ash_color_provider_->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
}

SkColor AppListColorProviderImpl::GetFolderTitleTextColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
}

SkColor AppListColorProviderImpl::GetFolderHintTextColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary);
}

SkColor AppListColorProviderImpl::GetFolderNameBackgroundColor(
    bool active) const {
  if (!active)
    return SK_ColorTRANSPARENT;

  AshColorProvider::RippleAttributes ripple_attributes =
      ash_color_provider_->GetRippleAttributes(GetAppListBackgroundColor());
  return SkColorSetA(ripple_attributes.base_color,
                     ripple_attributes.inkdrop_opacity * 255);
}

SkColor AppListColorProviderImpl::GetContentsBackgroundColor() const {
  return ash_color_provider_->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

SkColor AppListColorProviderImpl::GetSeparatorColor() const {
  return ash_color_provider_->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor);
}

SkColor AppListColorProviderImpl::GetSearchResultViewInkDropColor() const {
  AshColorProvider::RippleAttributes ripple_attributes =
      ash_color_provider_->GetRippleAttributes(
          GetSearchBoxCardBackgroundColor());
  return SkColorSetA(ripple_attributes.base_color,
                     ripple_attributes.inkdrop_opacity * 255);
}

SkColor AppListColorProviderImpl::GetSearchResultViewHighlightColor() const {
  AshColorProvider::RippleAttributes ripple_attributes =
      ash_color_provider_->GetRippleAttributes(
          GetSearchBoxCardBackgroundColor());
  return SkColorSetA(ripple_attributes.base_color,
                     ripple_attributes.highlight_opacity * 255);
}

float AppListColorProviderImpl::GetFolderBackgrounBlurSigma() const {
  return static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault);
}

}  // namespace ash
