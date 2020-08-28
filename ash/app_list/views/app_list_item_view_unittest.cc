// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_item_view.h"

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/test/views_test_base.h"

namespace ash {

class AppListItemViewTest : public views::ViewsTestBase {
 public:
  AppListItemViewTest() {
    scoped_feature_list_.InitWithFeatures({features::kNotificationIndicator},
                                          {});
  }
  ~AppListItemViewTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    delegate_ = std::make_unique<test::AppListTestViewDelegate>();
    app_list_view_ = new AppListView(delegate_.get());
    app_list_view_->InitView(GetContext());
  }

  AppListItemView* CreateAppListItemView() {
    AppsGridView* apps_grid_view = app_list_view_->app_list_main_view()
                                       ->contents_view()
                                       ->apps_container_view()
                                       ->apps_grid_view();
    return new AppListItemView(apps_grid_view, new AppListItem("Item 0"),
                               delegate_.get(), false);
  }

 private:
  AppListView* app_list_view_ = nullptr;
  std::unique_ptr<test::AppListTestViewDelegate> delegate_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the notification indicator has a color which is calculated
// correctly when an icon is set for the AppListItemView.
TEST_F(AppListItemViewTest, NotificatonBadgeColor) {
  AppListItemView* view = CreateAppListItemView();

  const int width = 64;
  const int height = 64;

  SkBitmap all_black_icon;
  all_black_icon.allocN32Pixels(width, height);
  all_black_icon.eraseColor(SK_ColorBLACK);

  view->SetIcon(gfx::ImageSkia::CreateFrom1xBitmap(all_black_icon));

  // For an all black icon, a white notification badge is expected, since there
  // is no other light vibrant color to get from the icon.
  EXPECT_EQ(SK_ColorWHITE, view->GetNotificationIndicatorColorForTest());

  // Create an icon that is half kGoogleRed300 and half kGoogleRed600.
  SkBitmap red_icon;
  red_icon.allocN32Pixels(width, height);
  red_icon.eraseColor(gfx::kGoogleRed300);
  red_icon.erase(gfx::kGoogleRed600, {0, 0, width, height / 2});

  view->SetIcon(gfx::ImageSkia::CreateFrom1xBitmap(red_icon));

  // For the red icon, the notification badge should calculate and use the
  // kGoogleRed300 color as the light vibrant color taken from the icon.
  EXPECT_EQ(gfx::kGoogleRed300, view->GetNotificationIndicatorColorForTest());
}

}  //  namespace ash
