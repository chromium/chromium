// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/app_list_bubble_assistant_page.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace {

// TODO(crbug.com/1229427): Merge with AssistantPageViewTest and parameterize
// based on bubble type.
class AppListBubbleAssistantPageTest : public AssistantAshTestBase {
 public:
  AppListBubbleAssistantPageTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~AppListBubbleAssistantPageTest() override = default;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppListBubbleAssistantPageTest, PressingAssistantKeyShowsAssistantPage) {
  ShowAssistantUi(AssistantEntryPoint::kHotkey);

  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible());
  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleAssistantPage()->GetVisible());
}

TEST_F(AppListBubbleAssistantPageTest,
       OpeningLauncherThenPressingAssistantKeyShowsAssistantPage) {
  OpenLauncher();
  ShowAssistantUi(AssistantEntryPoint::kHotkey);

  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleAssistantPage()->GetVisible());
}

TEST_F(AppListBubbleAssistantPageTest,
       PressingAssistantKeyTwiceClosesLauncher) {
  ShowAssistantUi(AssistantEntryPoint::kHotkey);
  CloseAssistantUi(AssistantExitPoint::kHotkey);

  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());
}

}  // namespace
}  // namespace ash
