// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_search_page.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {
namespace {

class AppListBubbleSearchPageTest : public AshTestBase {
 public:
  AppListBubbleSearchPageTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~AppListBubbleSearchPageTest() override = default;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppListBubbleSearchPageTest, ResultContainerIsVisible) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  // Press a key to start a search.
  PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE);

  // The single result container is visible.
  std::vector<SearchResultContainerView*> result_containers =
      test_helper->GetBubbleSearchPage()->result_container_views_for_test();
  ASSERT_EQ(result_containers.size(), 1u);
  EXPECT_TRUE(result_containers[0]->GetVisible());
}

}  // namespace
}  // namespace ash
