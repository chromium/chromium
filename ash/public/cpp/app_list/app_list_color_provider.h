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

class ASH_PUBLIC_EXPORT AppListColorProvider {
 public:
  // Returns the singleton instance.
  static AppListColorProvider* Get();

  virtual SkColor GetPageSwitcherButtonColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetFolderBackgroundColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetFolderNameBorderColor(
      bool active,
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetFolderNotificationBadgeColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetContentsBackgroundColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetGridBackgroundCardActiveColor(
      const views::Widget* app_list_widget) const = 0;
  virtual SkColor GetGridBackgroundCardInactiveColor(
      const views::Widget* app_list_widget) const = 0;
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

 protected:
  AppListColorProvider();
  virtual ~AppListColorProvider();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_COLOR_PROVIDER_H_
