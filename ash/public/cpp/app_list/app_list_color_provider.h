// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_COLOR_PROVIDER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_COLOR_PROVIDER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

constexpr SkColor kDeprecatedSearchBoxTextDefaultColor =
    SkColorSetRGB(0x33, 0x33, 0x33);

class ASH_PUBLIC_EXPORT AppListColorProvider {
 public:
  // Returns the singleton instance.
  static AppListColorProvider* Get();

  virtual SkColor GetExpandArrowInkDropBaseColor() const = 0;
  virtual SkColor GetExpandArrowIconBaseColor() const = 0;
  virtual SkColor GetExpandArrowIconBackgroundColor() const = 0;
  virtual SkColor GetAppListBackgroundColor() const = 0;
  virtual SkColor GetSearchBoxBackgroundColor() const = 0;
  virtual SkColor GetSearchBoxPlaceholderTextColor() const = 0;
  virtual SkColor GetSearchBoxTextColor(SkColor default_color) const = 0;
  virtual SkColor GetSearchBoxSecondaryTextColor(
      SkColor default_color) const = 0;
  virtual SkColor GetSuggestionChipBackgroundColor() const = 0;
  virtual SkColor GetSuggestionChipTextColor() const = 0;
  virtual SkColor GetAppListItemTextColor() const = 0;
  virtual SkColor GetPageSwitcherButtonColor(
      bool is_root_app_grid_page_switcher) const = 0;
  virtual SkColor GetPageSwitcherInkDropBaseColor(
      bool is_root_app_grid_page_switcher) const = 0;
  virtual SkColor GetPageSwitcherInkDropHighlightColor(
      bool is_root_app_grid_page_switcher) const = 0;
  virtual SkColor GetSearchBoxIconColor(SkColor default_color) const = 0;
  virtual SkColor GetSearchBoxCardBackgroundColor() const = 0;
  virtual SkColor GetFolderBackgroundColor(SkColor default_color) const = 0;
  virtual SkColor GetFolderTitleTextColor(SkColor default_color) const = 0;
  virtual SkColor GetFolderHintTextColor() const = 0;
  virtual SkColor GetFolderNameBackgroundColor(bool active) const = 0;
  virtual SkColor GetFolderNameBorderColor(bool active) const = 0;
  virtual SkColor GetFolderNameSelectionColor() const = 0;
  virtual SkColor GetContentsBackgroundColor() const = 0;
  virtual SkColor GetSeparatorColor() const = 0;
  virtual SkColor GetSearchResultViewHighlightColor() const = 0;
  virtual SkColor GetSearchResultViewInkDropColor() const = 0;
  virtual float GetFolderBackgrounBlurSigma() const = 0;

 protected:
  AppListColorProvider();
  virtual ~AppListColorProvider();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_COLOR_PROVIDER_H_
