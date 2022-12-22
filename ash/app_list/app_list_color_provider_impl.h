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
  SkColor GetPageSwitcherButtonColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderNotificationBadgeColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetGridBackgroundCardActiveColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetGridBackgroundCardInactiveColor(
      const views::Widget* app_list_widget) const override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_COLOR_PROVIDER_IMPL_H_
