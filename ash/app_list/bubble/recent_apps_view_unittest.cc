// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/recent_apps_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/bubble/app_list_bubble_apps_page.h"
#include "ash/app_list/bubble/app_list_bubble_view.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

void AddAppListItem(const std::string& id) {
  Shell::Get()->app_list_controller()->GetModel()->AddItem(
      std::make_unique<AppListItem>(id));
}

void AddSearchResult(const std::string& id, AppListSearchResultType type) {
  auto result = std::make_unique<TestSearchResult>();
  result->set_result_id(id);
  result->set_result_type(type);
  // TODO(crbug.com/1216662): Replace with a real display type after the ML team
  // gives us a way to query directly for recent apps.
  result->set_display_type(SearchResultDisplayType::kChip);
  Shell::Get()->app_list_controller()->GetSearchModel()->results()->Add(
      std::move(result));
}

void ShowAppList() {
  Shell::Get()->app_list_controller()->ShowAppList();
}

RecentAppsView* GetRecentAppsView() {
  return Shell::Get()
      ->app_list_controller()
      ->bubble_presenter_for_test()
      ->bubble_view_for_test()
      ->apps_page_for_test()
      ->recent_apps_for_test();
}

// An AppListClient that records some method calls.
class RecordingAppListClient : public TestAppListClient {
 public:
  void ActivateItem(int profile_id,
                    const std::string& id,
                    int event_flags) override {
    activate_item_count_++;
    activate_item_last_id_ = id;
  }

  int activate_item_count_ = 0;
  std::string activate_item_last_id_;
};

class RecentAppsViewTest : public AshTestBase {
 public:
  RecentAppsViewTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~RecentAppsViewTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->app_list_controller()->SetClient(&app_list_client_);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  RecordingAppListClient app_list_client_;
};

TEST_F(RecentAppsViewTest, CreatesIconsForApps) {
  AddAppListItem("id1");
  AddSearchResult("id1", AppListSearchResultType::kInstalledApp);
  AddAppListItem("id2");
  AddSearchResult("id2", AppListSearchResultType::kPlayStoreApp);
  AddAppListItem("id3");
  AddSearchResult("id3", AppListSearchResultType::kInstantApp);
  AddAppListItem("id4");
  AddSearchResult("id4", AppListSearchResultType::kInternalApp);

  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  EXPECT_EQ(view->children().size(), 4u);
}

TEST_F(RecentAppsViewTest, DoesNotCreateIconsForNonApps) {
  AddSearchResult("id1", AppListSearchResultType::kAnswerCard);
  AddSearchResult("id2", AppListSearchResultType::kFileChip);
  AddSearchResult("id3", AppListSearchResultType::kAssistantText);

  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  EXPECT_EQ(view->children().size(), 0u);
}

TEST_F(RecentAppsViewTest, DoesNotCreateIconForMismatchedId) {
  AddAppListItem("id");
  AddSearchResult("bad id", AppListSearchResultType::kInstalledApp);

  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  EXPECT_EQ(view->children().size(), 0u);
}

TEST_F(RecentAppsViewTest, ClickOnRecentApp) {
  AddAppListItem("id");
  AddSearchResult("id", AppListSearchResultType::kInstalledApp);

  ShowAppList();

  // Click on the first icon.
  RecentAppsView* view = GetRecentAppsView();
  ASSERT_FALSE(view->children().empty());
  views::View* icon = view->children()[0];
  GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // The item was activated.
  EXPECT_EQ(app_list_client_.activate_item_count_, 1);
  EXPECT_EQ(app_list_client_.activate_item_last_id_, "id");
}

// TODO(jamescook): Test context menus.

}  // namespace
}  // namespace ash
