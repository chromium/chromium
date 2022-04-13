// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_COLOR_PROVIDER_IMPL_H_
#define ASH_APP_LIST_APP_LIST_COLOR_PROVIDER_IMPL_H_

#include "ash/public/cpp/app_list/app_list_color_provider.h"

namespace ash {

class AshColorProvider;

class AppListColorProviderImpl : public AppListColorProvider {
 public:
  AppListColorProviderImpl();
  ~AppListColorProviderImpl() override;
  // AppListColorProvider:
  SkColor GetExpandArrowIconBaseColor() const override;
  SkColor GetExpandArrowIconBackgroundColor() const override;
  SkColor GetAppListBackgroundColor(bool is_tablet_mode,
                                    SkColor default_color) const override;
  SkColor GetSearchBoxBackgroundColor() const override;
  SkColor GetSearchBoxSecondaryTextColor(SkColor default_color) const override;
  SkColor GetSearchBoxTextColor(SkColor default_color) const override;
  SkColor GetSuggestionChipBackgroundColor() const override;
  SkColor GetSuggestionChipTextColor() const override;
  SkColor GetAppListItemTextColor(bool is_in_folder) const override;
  SkColor GetPageSwitcherButtonColor(
      bool is_root_app_grid_page_switcher) const override;
  SkColor GetSearchBoxIconColor(SkColor default_color) const override;
  SkColor GetSearchBoxCardBackgroundColor() const override;
  SkColor GetFolderBackgroundColor() const override;
  SkColor GetFolderBubbleColor() const override;
  SkColor GetFolderTitleTextColor() const override;
  SkColor GetFolderHintTextColor() const override;
  SkColor GetFolderNameBorderColor(bool active) const override;
  SkColor GetFolderNameSelectionColor() const override;
  SkColor GetContentsBackgroundColor() const override;
  SkColor GetGridBackgroundCardActiveColor() const override;
  SkColor GetGridBackgroundCardInactiveColor() const override;
  SkColor GetSeparatorColor() const override;
  SkColor GetFocusRingColor() const override;
  SkColor GetInkDropBaseColor(
      SkColor bg_color = gfx::kPlaceholderColor) const override;
  float GetInkDropOpacity(
      SkColor bg_color = gfx::kPlaceholderColor) const override;
  SkColor GetInvertedInkDropBaseColor(
      SkColor bg_color = gfx::kPlaceholderColor) const override;
  float GetInvertedInkDropOpacity(
      SkColor bg_color = gfx::kPlaceholderColor) const override;
  SkColor GetSearchResultViewHighlightColor() const override;
  SkColor GetTextColorURL() const override;

 private:
  bool ShouldUseDarkLightColors() const;
  // Unowned.
  AshColorProvider* const ash_color_provider_;
  // Whether feature DarkLightMode is enabled. Cached for efficiency.
  const bool is_dark_light_mode_enabled_;
  // Whether feature ProductivityLauncher is enabled. Cached for efficiency.
  const bool is_productivity_launcher_enabled_;
  // Whether feature BackgroundBlur is enabled. Cached for efficiency.
  const bool is_background_blur_enabled_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_COLOR_PROVIDER_IMPL_H_
