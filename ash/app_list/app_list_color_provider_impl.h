// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_COLOR_PROVIDER_IMPL_H_
#define ASH_APP_LIST_APP_LIST_COLOR_PROVIDER_IMPL_H_

#include "ash/public/cpp/app_list/app_list_color_provider.h"

namespace ash {

class AppListColorProviderImpl : public AppListColorProvider {
 public:
  AppListColorProviderImpl();
  ~AppListColorProviderImpl() override;
  // AppListColorProvider:
  SkColor GetSearchBoxBackgroundColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxSecondaryTextColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxSuggestionTextColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxTextColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetAppListItemTextColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetPageSwitcherButtonColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxIconColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxCardBackgroundColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderBackgroundColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderTitleTextColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderHintTextColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderNameBorderColor(
      bool active,
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderNameSelectionColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderNotificationBadgeColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetContentsBackgroundColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetGridBackgroundCardActiveColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetGridBackgroundCardInactiveColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFocusRingColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetInkDropBaseColor(
      const views::Widget* app_list_widget,
      SkColor bg_color = gfx::kPlaceholderColor) const override;
  float GetInkDropOpacity(
      const views::Widget* app_list_widget,
      SkColor bg_color = gfx::kPlaceholderColor) const override;
  SkColor GetSearchResultViewHighlightColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetTextColorURL(const views::Widget* app_list_widget) const override;

 private:
  // Whether feature BackgroundBlur is enabled. Cached for efficiency.
  const bool is_background_blur_enabled_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_COLOR_PROVIDER_IMPL_H_
