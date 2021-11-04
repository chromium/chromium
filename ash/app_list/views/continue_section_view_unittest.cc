// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_section_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {
namespace {

using test::AppListTestViewDelegate;

void AddSearchResultToModel(const std::string& id,
                            AppListSearchResultType type,
                            SearchModel* model) {
  auto result = std::make_unique<TestSearchResult>();
  result->set_result_id(id);
  result->set_result_type(type);
  result->set_display_type(SearchResultDisplayType::kContinue);
  model->results()->Add(std::move(result));
}

void ShowAppList() {
  Shell::Get()->app_list_controller()->ShowAppList();
}

class ContinueSectionViewTest : public AshTestBase {
 public:
  ContinueSectionViewTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~ContinueSectionViewTest() override = default;

  // testing::Test:
  void SetUp() override { AshTestBase::SetUp(); }

  ContinueSectionView* GetContinueSectionView() {
    return GetAppListTestHelper()->GetContinueSectionView();
  }

  void AddSearchResult(const std::string& id, AppListSearchResultType type) {
    AddSearchResultToModel(id, type,
                           AppListModelProvider::Get()->search_model());
  }

  void RemoveSearchResultAt(size_t index) {
    GetAppListTestHelper()->GetSearchResults()->RemoveAt(index);
  }

  SearchModel::SearchResults* GetResults() {
    return GetAppListTestHelper()->GetSearchResults();
  }

  SearchBoxView* GetSearchBoxView() {
    return GetAppListTestHelper()->GetBubbleSearchBoxView();
  }

  ContinueTaskView* GetResultViewAt(int index) {
    return GetContinueSectionView()->GetTaskViewAtForTesting(index);
  }

  std::vector<std::string> GetResultIds() {
    const size_t result_count =
        GetContinueSectionView()->GetTasksSuggestionsCount();
    std::vector<std::string> ids;
    for (size_t i = 0; i < result_count; ++i)
      ids.push_back(GetResultViewAt(i)->result()->id());
    return ids;
  }

  void VerifyResultViewsUpdated() {
    // Wait for the view to update any pending SearchResults.
    base::RunLoop().RunUntilIdle();

    SearchModel::SearchResults* results = GetResults();
    for (size_t i = 0; i < results->item_count(); ++i)
      EXPECT_EQ(results->GetItemAt(i), GetResultViewAt(i)->result());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContinueSectionViewTest, CreatesViewsForTasks) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);

  ShowAppList();

  ContinueSectionView* view = GetContinueSectionView();
  EXPECT_EQ(view->GetTasksSuggestionsCount(), 2u);
}

TEST_F(ContinueSectionViewTest, VerifyAddedViewsOrder) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  ShowAppList();
  VerifyResultViewsUpdated();

  ContinueSectionView* view = GetContinueSectionView();
  ASSERT_EQ(view->GetTasksSuggestionsCount(), 3u);
  EXPECT_EQ(GetResultViewAt(0)->result()->id(), "id1");
  EXPECT_EQ(GetResultViewAt(1)->result()->id(), "id2");
  EXPECT_EQ(GetResultViewAt(2)->result()->id(), "id3");
}

TEST_F(ContinueSectionViewTest, ModelObservers) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  ShowAppList();

  VerifyResultViewsUpdated();

  // Remove from end.
  GetResults()->DeleteAt(2);
  VerifyResultViewsUpdated();

  // Insert a new result.
  AddSearchResult("id4", AppListSearchResultType::kFileChip);
  VerifyResultViewsUpdated();

  // Delete from start.
  GetResults()->DeleteAt(0);
  VerifyResultViewsUpdated();
}

TEST_F(ContinueSectionViewTest, HideContinueSectionWhenResultRemoved) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  ShowAppList();
  VerifyResultViewsUpdated();
  EXPECT_TRUE(GetContinueSectionView()->GetVisible());

  RemoveSearchResultAt(2);
  VerifyResultViewsUpdated();

  EXPECT_FALSE(GetContinueSectionView()->GetVisible());
}

TEST_F(ContinueSectionViewTest, ShowContinueSectionWhenResultAdded) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);

  ShowAppList();
  VerifyResultViewsUpdated();
  EXPECT_FALSE(GetContinueSectionView()->GetVisible());

  AddSearchResult("id3", AppListSearchResultType::kDriveChip);
  VerifyResultViewsUpdated();

  EXPECT_TRUE(GetContinueSectionView()->GetVisible());
}

TEST_F(ContinueSectionViewTest, ClickOpensSearchResult) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  ShowAppList();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);

  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetEventGenerator()->MoveMouseTo(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_F(ContinueSectionViewTest, TapOpensSearchResult) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  ShowAppList();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);

  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetEventGenerator()->GestureTapAt(
      continue_task_view->GetBoundsInScreen().CenterPoint());

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_F(ContinueSectionViewTest, PressEnterOpensSearchResult) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  ShowAppList();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());

  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_TRUE(continue_task_view->HasFocus());

  PressAndReleaseKey(ui::VKEY_RETURN);

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_F(ContinueSectionViewTest, RightClickOpensContextMenu) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  ShowAppList();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetEventGenerator()->MoveMouseTo(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickRightButton();
  EXPECT_TRUE(continue_task_view->IsMenuShowing());
}

TEST_F(ContinueSectionViewTest, OpenWithContextMenuOption) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  ShowAppList();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetEventGenerator()->MoveMouseTo(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickRightButton();
  continue_task_view->ExecuteCommand(ContinueTaskCommandId::kOpenResult,
                                     ui::EventFlags::EF_NONE);

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_F(ContinueSectionViewTest, ResultRemovedContextMenuCloses) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);
  AddSearchResult("id4", AppListSearchResultType::kFileChip);

  ShowAppList();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(3);
  EXPECT_EQ(continue_task_view->result()->id(), "id4");

  GetEventGenerator()->MoveMouseTo(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickRightButton();
  EXPECT_TRUE(continue_task_view->IsMenuShowing());

  RemoveSearchResultAt(3);
  VerifyResultViewsUpdated();

  EXPECT_FALSE(continue_task_view->IsMenuShowing());
}

TEST_F(ContinueSectionViewTest, UpdateAppsOnModelChange) {
  AddSearchResult("id11", AppListSearchResultType::kFileChip);
  AddSearchResult("id12", AppListSearchResultType::kDriveChip);
  AddSearchResult("id13", AppListSearchResultType::kDriveChip);
  AddSearchResult("id14", AppListSearchResultType::kFileChip);
  ShowAppList();

  EXPECT_EQ(std::vector<std::string>({"id11", "id12", "id13", "id14"}),
            GetResultIds());

  // Update active model, and make sure the shown results view get updated
  // accordingly.
  auto model_override = std::make_unique<test::AppListTestModel>();
  auto search_model_override = std::make_unique<SearchModel>();

  AddSearchResultToModel("id21", AppListSearchResultType::kFileChip,
                         search_model_override.get());
  AddSearchResultToModel("id22", AppListSearchResultType::kFileChip,
                         search_model_override.get());
  AddSearchResultToModel("id23", AppListSearchResultType::kFileChip,
                         search_model_override.get());

  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get());
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(std::vector<std::string>({"id21", "id22", "id23"}), GetResultIds());

  // Tap a result, and verify it gets activated.
  GetEventGenerator()->GestureTapAt(
      GetResultViewAt(2)->GetBoundsInScreen().CenterPoint());

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id23", client->last_opened_search_result());

  // Results should be cleared if app list models get reset.
  Shell::Get()->app_list_controller()->ClearActiveModel();
  EXPECT_EQ(std::vector<std::string>{}, GetResultIds());
}

}  // namespace
}  // namespace ash
