// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/launcher_search_iph_view.h"

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/chip_view.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"

namespace ash {

namespace {

using LauncherSearchIphViewTest = AssistantAshTestBase;

}  // namespace

TEST_F(LauncherSearchIphViewTest,
       ShouldShuffleQueriesWhenShowingAssistantPage) {
  base::test::ScopedFeatureList scoped_feature_list(
      assistant::features::kEnableAssistantLearnMore);

  ShowAssistantUi();
  LauncherSearchIphView* iph_view = static_cast<LauncherSearchIphView*>(
      page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  std::vector<std::u16string> queries_1;
  for (auto chip : iph_view->GetChipsForTesting()) {
    queries_1.emplace_back(chip->GetText());
  }

  // Close and show Assistant UI again.
  CloseAssistantUi();
  ShowAssistantUi();
  std::vector<std::u16string> queries_2;
  for (auto chip : iph_view->GetChipsForTesting()) {
    queries_2.emplace_back(chip->GetText());
  }

  ASSERT_EQ(queries_1.size(), queries_2.size());
  EXPECT_NE(queries_1, queries_2);
}

TEST_F(LauncherSearchIphViewTest, ShouldShuffleQueriesWhenVisible) {
  auto iph_view = std::make_unique<LauncherSearchIphView>(
      /*delegate=*/nullptr, /*is_in_tablet_mode=*/false,
      /*scoped_iph_session=*/nullptr, /*show_assistant_chip=*/false);

  std::vector<std::u16string> queries_1;
  for (auto chip : iph_view->GetChipsForTesting()) {
    queries_1.emplace_back(chip->GetText());
  }

  iph_view->SetVisible(false);
  iph_view->SetVisible(true);
  std::vector<std::u16string> queries_2;
  for (auto chip : iph_view->GetChipsForTesting()) {
    queries_2.emplace_back(chip->GetText());
  }

  ASSERT_EQ(queries_1.size(), queries_2.size());
  EXPECT_NE(queries_1, queries_2);
}

}  // namespace ash
