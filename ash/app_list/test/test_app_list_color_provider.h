// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_TEST_APP_LIST_COLOR_PROVIDER_H_
#define ASH_APP_LIST_TEST_TEST_APP_LIST_COLOR_PROVIDER_H_

#include "ash/public/cpp/app_list/app_list_color_provider.h"

namespace ash {

class TestAppListColorProvider : public AppListColorProvider {
 public:
  TestAppListColorProvider() = default;
  ~TestAppListColorProvider() override = default;

 public:
  // AppListColorProvider:
  SkColor GetExpandArrowInkDropBaseColor() const override;
  SkColor GetExpandArrowIconBaseColor() const override;
  SkColor GetExpandArrowIconBackgroundColor() const override;
  SkColor GetAppListBackgroundColor() const override;
  SkColor GetSearchBoxBackgroundColor() const override;
  SkColor GetSearchBoxPlaceholderTextColor() const override;
  SkColor GetSearchBoxTextColor(SkColor default_color) const override;
  SkColor GetSearchBoxSecondaryTextColor(SkColor default_color) const override;
  SkColor GetSuggestionChipBackgroundColor() const override;
  SkColor GetSuggestionChipTextColor() const override;
  SkColor GetAppListItemTextColor() const override;
  SkColor GetPageSwitcherButtonColor(
      bool is_root_app_grid_page_switcher) const override;
  SkColor GetPageSwitcherInkDropBaseColor(
      bool is_root_app_grid_page_switcher) const override;
  SkColor GetPageSwitcherInkDropHighlightColor(
      bool is_root_app_grid_page_switcher) const override;
  SkColor GetSearchBoxIconColor(SkColor default_color) const override;
  SkColor GetSearchBoxCardBackgroundColor() const override;
  SkColor GetFolderBackgroundColor(SkColor default_color) const override;
  SkColor GetFolderTitleTextColor(SkColor default_color) const override;
  SkColor GetFolderHintTextColor() const override;
  SkColor GetFolderNameBackgroundColor(bool active) const override;
  SkColor GetFolderNameBorderColor(bool active) const override;
  SkColor GetFolderNameSelectionColor() const override;
  SkColor GetContentsBackgroundColor() const override;
  SkColor GetSeparatorColor() const override;
  SkColor GetSearchResultViewHighlightColor() const override;
  SkColor GetSearchResultViewInkDropColor() const override;
  float GetFolderBackgrounBlurSigma() const override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_TEST_APP_LIST_COLOR_PROVIDER_H_
