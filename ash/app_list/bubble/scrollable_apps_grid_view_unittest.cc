// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/scrollable_apps_grid_view.h"

#include <memory>
#include <string>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/bubble/app_list_bubble_apps_page.h"
#include "ash/app_list/bubble/app_list_bubble_view.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace {

ScrollableAppsGridView* GetScrollableAppsGridView() {
  return Shell::Get()
      ->app_list_controller()
      ->bubble_presenter_for_test()
      ->bubble_view_for_test()
      ->apps_page_for_test()
      ->scrollable_apps_grid_view_for_test();
}

void ShowAppList() {
  Shell::Get()->app_list_controller()->ShowAppList();
}

void AddAppListItem(const std::string& id) {
  Shell::Get()->app_list_controller()->GetModel()->AddItem(
      std::make_unique<AppListItem>(id));
}

}  // namespace

class ScrollableAppsGridViewTest : public AshTestBase {
 public:
  ScrollableAppsGridViewTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~ScrollableAppsGridViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->app_list_controller()->SetClient(&app_list_client_);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestAppListClient app_list_client_;
};

TEST_F(ScrollableAppsGridViewTest, ClickOnApp) {
  AddAppListItem("id");

  ShowAppList();

  // Click on the first icon.
  ScrollableAppsGridView* view = GetScrollableAppsGridView();
  views::View* icon = view->GetItemViewAt(0);
  GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // The item was activated.
  EXPECT_EQ(1, app_list_client_.activate_item_count());
  EXPECT_EQ("id", app_list_client_.activate_item_last_id());
}

}  // namespace ash
