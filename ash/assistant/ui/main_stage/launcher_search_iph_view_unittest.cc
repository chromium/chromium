// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/launcher_search_iph_view.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/chip_view.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

using LauncherSearchIphViewTest = AssistantAshTestBase;

// TODO(b/317900261): Use ash::ViewDrawnWaiter.
bool IsDrawn(views::View* view) {
  return view->IsDrawn() && !view->size().IsEmpty();
}

class ViewDrawnWaiter : public views::ViewObserver {
 public:
  ViewDrawnWaiter() = default;
  ~ViewDrawnWaiter() override = default;

  void Wait(views::View* view) {
    if (IsDrawn(view)) {
      return;
    }

    view_observer_.Observe(view);
    wait_loop_.Run();
  }

 private:
  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* view) override {
    if (IsDrawn(view)) {
      wait_loop_.Quit();
    }
  }

  base::RunLoop wait_loop_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observer_{
      this};
};

}  // namespace

TEST_F(LauncherSearchIphViewTest,
       ShouldShuffleQueriesWhenShowingAssistantPage) {
  base::test::ScopedFeatureList scoped_feature_list(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

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
      /*scoped_iph_session=*/nullptr,
      /*location=*/LauncherSearchIphView::UiLocation::kAssistantPage);

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

TEST_F(LauncherSearchIphViewTest, ShowFourChipsInAssistantPage) {
  base::test::ScopedFeatureList scoped_feature_list(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

  ShowAssistantUi();
  LauncherSearchIphView* iph_view = static_cast<LauncherSearchIphView*>(
      page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  int visible_chips = 0;
  for (auto chip : iph_view->GetChipsForTesting()) {
    if (chip->GetVisible()) {
      visible_chips++;
    }
  }
  EXPECT_EQ(4, visible_chips);
}

TEST_F(LauncherSearchIphViewTest, ShowTwoChipsInAssistantPage) {
  base::test::ScopedFeatureList scoped_feature_list(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

  // Set a testing text to only show two chips.
  std::u16string testing_text = u"Long text for two chips";
  LauncherSearchIphView::SetChipTextForTesting(testing_text);

  ShowAssistantUi();
  LauncherSearchIphView* iph_view = static_cast<LauncherSearchIphView*>(
      page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  int visible_chips = 0;
  for (auto chip : iph_view->GetChipsForTesting()) {
    if (chip->GetVisible()) {
      visible_chips++;
    }
  }
  EXPECT_EQ(2, visible_chips);
}

TEST_F(LauncherSearchIphViewTest, ShowOneChipsInAssistantPage) {
  base::test::ScopedFeatureList scoped_feature_list(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

  // Set a testing text to only show one chip.
  std::u16string testing_text = u"Very long text to only show one chip";
  LauncherSearchIphView::SetChipTextForTesting(testing_text);

  ShowAssistantUi();
  LauncherSearchIphView* iph_view = static_cast<LauncherSearchIphView*>(
      page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  int visible_chips = 0;
  for (auto chip : iph_view->GetChipsForTesting()) {
    if (chip->GetVisible()) {
      visible_chips++;
    }
  }
  EXPECT_EQ(1, visible_chips);
}

TEST_F(LauncherSearchIphViewTest, AtLeastShowOneChipsInAssistantPage) {
  base::test::ScopedFeatureList scoped_feature_list(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

  // Set a testing text to only show one chip even the text cannot fix the
  // width.
  std::u16string testing_text =
      u"Very very very very long text cannot fix the width but still show one "
      u"chip";
  LauncherSearchIphView::SetChipTextForTesting(testing_text);

  ShowAssistantUi();
  LauncherSearchIphView* iph_view = static_cast<LauncherSearchIphView*>(
      page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  int visible_chips = 0;
  for (auto chip : iph_view->GetChipsForTesting()) {
    if (chip->GetVisible()) {
      visible_chips++;
    }
  }
  EXPECT_EQ(1, visible_chips);
}

TEST_F(LauncherSearchIphViewTest,
       ShowIphWhenClickingAssistantButtonInSearchBox) {
  GetAppListTestHelper()->search_model()->SetWouldTriggerLauncherSearchIph(
      true);
  GetAppListTestHelper()->ShowAppList();

  SearchBoxView* search_box_view =
      GetAppListTestHelper()->GetBubbleSearchBoxView();
  LeftClickOn(search_box_view->assistant_button());

  LauncherSearchIphView* iph_view = static_cast<LauncherSearchIphView*>(
      search_box_view->GetViewByID(LauncherSearchIphView::ViewId::kSelf));
  EXPECT_TRUE(!!iph_view);
  EXPECT_TRUE(iph_view->GetVisible());
}

TEST_F(LauncherSearchIphViewTest,
       ShowThreeQueryChipsAndAssistantChipInSearchBox) {
  // Set a testing text to show three chips.
  std::u16string testing_text = u"Text";
  LauncherSearchIphView::SetChipTextForTesting(testing_text);

  GetAppListTestHelper()->search_model()->SetWouldTriggerLauncherSearchIph(
      true);
  GetAppListTestHelper()->ShowAppList();

  SearchBoxView* search_box_view =
      GetAppListTestHelper()->GetBubbleSearchBoxView();
  LeftClickOn(search_box_view->assistant_button());

  LauncherSearchIphView* iph_view = static_cast<LauncherSearchIphView*>(
      search_box_view->GetViewByID(LauncherSearchIphView::ViewId::kSelf));
  ViewDrawnWaiter().Wait(iph_view);

  int visible_chips = 0;
  for (auto chip : iph_view->GetChipsForTesting()) {
    if (chip->GetVisible()) {
      visible_chips++;
    }
  }
  EXPECT_EQ(3, visible_chips);

  EXPECT_TRUE(!!iph_view->GetAssistantButtonForTesting());
  EXPECT_TRUE(iph_view->GetAssistantButtonForTesting()->GetVisible());
}

TEST_F(LauncherSearchIphViewTest, ShowOneQueryChipAndAssistantChipInSearchBox) {
  // Set a testing text to only show one chip.
  std::u16string testing_text = u"Very long text to show only one chip";
  LauncherSearchIphView::SetChipTextForTesting(testing_text);

  GetAppListTestHelper()->search_model()->SetWouldTriggerLauncherSearchIph(
      true);
  GetAppListTestHelper()->ShowAppList();

  SearchBoxView* search_box_view =
      GetAppListTestHelper()->GetBubbleSearchBoxView();
  LeftClickOn(search_box_view->assistant_button());

  LauncherSearchIphView* iph_view = static_cast<LauncherSearchIphView*>(
      search_box_view->GetViewByID(LauncherSearchIphView::ViewId::kSelf));
  ViewDrawnWaiter().Wait(iph_view);

  int visible_chips = 0;
  for (auto chip : iph_view->GetChipsForTesting()) {
    if (chip->GetVisible()) {
      visible_chips++;
    }
  }
  EXPECT_EQ(1, visible_chips);

  EXPECT_TRUE(!!iph_view->GetAssistantButtonForTesting());
  EXPECT_TRUE(iph_view->GetAssistantButtonForTesting()->GetVisible());
}

TEST_F(LauncherSearchIphViewTest, AtLeastShowOneQueryChipInSearchBox) {
  // Set a testing text to only show one chip even the text cannot fix the
  // width.
  std::u16string testing_text =
      u"Very very very very long text cannot fix the width but still show one "
      u"chip";
  LauncherSearchIphView::SetChipTextForTesting(testing_text);

  GetAppListTestHelper()->search_model()->SetWouldTriggerLauncherSearchIph(
      true);
  GetAppListTestHelper()->ShowAppList();

  SearchBoxView* search_box_view =
      GetAppListTestHelper()->GetBubbleSearchBoxView();
  LeftClickOn(search_box_view->assistant_button());

  LauncherSearchIphView* iph_view = static_cast<LauncherSearchIphView*>(
      search_box_view->GetViewByID(LauncherSearchIphView::ViewId::kSelf));
  ViewDrawnWaiter().Wait(iph_view);

  int visible_chips = 0;
  for (auto chip : iph_view->GetChipsForTesting()) {
    if (chip->GetVisible()) {
      visible_chips++;
    }
  }
  EXPECT_EQ(1, visible_chips);

  EXPECT_TRUE(!!iph_view->GetAssistantButtonForTesting());
  EXPECT_TRUE(iph_view->GetAssistantButtonForTesting()->GetVisible());
}

}  // namespace ash
