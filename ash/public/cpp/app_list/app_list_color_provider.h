// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_COLOR_PROVIDER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_COLOR_PROVIDER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/widget/widget.h"

namespace ash {

constexpr SkColor kDeprecatedSearchBoxTextDefaultColor =
    SkColorSetRGB(0x33, 0x33, 0x33);

constexpr SkColor kDeprecatedSearchBoxPlaceholderTextColor =
    SkColorSetARGB(0xDE, 0x00, 0x00, 0x00);

class ASH_PUBLIC_EXPORT AppListColorProvider {
 public:
  // Returns the singleton instance.
  static AppListColorProvider* Get();

  // |default_color| will be used when dark/light mode is disabled.
  virtual SkColor GetAppListBackgroundColor(
      bool is_tablet_mode,
      SkColor default_color,
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetSearchBoxBackgroundColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetSearchBoxTextColor(
      SkColor default_color,
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetSearchBoxSecondaryTextColor(
      SkColor default_color,
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetSearchBoxSuggestionTextColor(
      SkColor default_color,
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetSuggestionChipBackgroundColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetSuggestionChipTextColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetAppListItemTextColor(
      bool is_in_folder,
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetPageSwitcherButtonColor(
      bool is_root_app_grid_page_switcher,
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetSearchBoxIconColor(
      SkColor default_color,
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetSearchBoxCardBackgroundColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetFolderBackgroundColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetFolderTitleTextColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetFolderHintTextColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetFolderNameBorderColor(
      bool active,
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetFolderNameSelectionColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetFolderNotificationBadgeColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetContentsBackgroundColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetGridBackgroundCardActiveColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetGridBackgroundCardInactiveColor(
      const views::Widget* app_list_widget) const = 0;
  virtual ui::ColorId GetSeparatorColorId() const = 0;
  virtual SkColor GetFocusRingColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetInkDropBaseColor(
      const views::Widget* app_list_widget,
      SkColor bg_color = gfx::kPlaceholderColor) const = 0;
  virtual float GetInkDropOpacity(
      const views::Widget* app_list_widget,
      SkColor bg_color = gfx::kPlaceholderColor) const = 0;
  virtual SkColor GetSearchResultViewHighlightColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetTextColorURL(
      const views::Widget* app_list_widget) const = 0;

 protected:
  AppListColorProvider();
  virtual ~AppListColorProvider();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_COLOR_PROVIDER_H_
