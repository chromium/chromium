// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_section_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

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

class ContinueSectionViewTest : public AshTestBase {
 public:
  ContinueSectionViewTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~ContinueSectionViewTest() override = default;

  // testing::Test:
  void SetUp() override { AshTestBase::SetUp(); }

  ContinueSectionView* GetContinueSectionView() {
    return GetAppListTestHelper()->GetContinueSectionView();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContinueSectionViewTest, CreatesViewsForTasks) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);

  ShowAppList();

  ContinueSectionView* view = GetContinueSectionView();
  EXPECT_EQ(view->GetTasksSuggestionsCount(), 2u);
}

TEST_F(ContinueSectionViewTest, DoesNotCreateViewsForNonTasks) {
  AddSearchResult("id1", AppListSearchResultType::kInstalledApp);
  AddSearchResult("id2", AppListSearchResultType::kPlayStoreApp);
  AddSearchResult("id3", AppListSearchResultType::kInstantApp);
  AddSearchResult("id4", AppListSearchResultType::kInternalApp);
  AddSearchResult("id5", AppListSearchResultType::kAnswerCard);
  AddSearchResult("id6", AppListSearchResultType::kAssistantText);

  ShowAppList();

  ContinueSectionView* view = GetContinueSectionView();
  EXPECT_EQ(view->GetTasksSuggestionsCount(), 0u);
}

TEST_F(ContinueSectionViewTest, VerifyAddedViewsOrder) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  ShowAppList();

  ContinueSectionView* view = GetContinueSectionView();
  ASSERT_EQ(view->GetTasksSuggestionsCount(), 3u);
  EXPECT_EQ(view->GetTaskViewAtForTesting(0)->result()->id(), "id1");
  EXPECT_EQ(view->GetTaskViewAtForTesting(1)->result()->id(), "id2");
  EXPECT_EQ(view->GetTaskViewAtForTesting(2)->result()->id(), "id3");
}

}  // namespace
}  // namespace ash
