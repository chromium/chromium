// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_chip_carousel.h"

#include <vector>

#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_features.h"
#include "ash/system/focus_mode/focus_mode_tasks_provider.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/rtl.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

MATCHER_P(LabelMatcher, task, "") {
  return static_cast<views::LabelButton*>(arg)->GetText() == task;
}

std::vector<std::string> kTestTaskTitles = {"Preparing for I485 form",
                                            "Podcast interview Script",
                                            "Book a flight to Seoul"};

constexpr int kWidgetWidth = 320;
constexpr float kGradientWidth = 16;

}  // namespace

namespace ash {

class FocusModeChipCarouselTest : public AshTestBase {
 public:
  FocusModeChipCarouselTest() : scoped_feature_(features::kFocusMode) {}
  ~FocusModeChipCarouselTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetBounds(gfx::Rect(/*width=*/kWidgetWidth, /*height=*/48));

    focus_mode_chip_carousel_ = widget_->SetContentsView(
        std::make_unique<FocusModeChipCarousel>(base::DoNothing()));
  }

  void TearDown() override {
    focus_mode_chip_carousel_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  std::vector<FocusModeTask> GetTasks(const std::vector<std::string>& titles) {
    std::vector<FocusModeTask> tasks;

    base::Time updated = base::Time::Now();
    for (size_t i = 0; i != titles.size(); ++i) {
      FocusModeTask& task = tasks.emplace_back();
      task.task_id = {.list_id = "task_list_id", .id = base::NumberToString(i)};
      task.title = titles[i];
      task.updated = updated - base::Seconds(i);
    }

    return tasks;
  }

  FocusModeChipCarousel* focus_mode_chip_carousel() {
    return focus_mode_chip_carousel_;
  }

  views::ScrollView* GetScrollView() {
    return focus_mode_chip_carousel_->GetScrollViewForTesting();
  }

  views::View* GetScrollContents() {
    return focus_mode_chip_carousel_->GetScrollViewForTesting()->contents();
  }

  views::ImageButton* GetLeftOverflowIcon() {
    return focus_mode_chip_carousel_->left_overflow_icon_;
  }

  views::ImageButton* GetRightOverflowIcon() {
    return focus_mode_chip_carousel_->right_overflow_icon_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<FocusModeChipCarousel> focus_mode_chip_carousel_;
};

// Tests that the task list displays the list of tasks.
TEST_F(FocusModeChipCarouselTest, ChipCarouselPopulates) {
  EXPECT_FALSE(focus_mode_chip_carousel()->HasTasks());
  auto validate_tasks = [&](const std::vector<std::string> task_titles) {
    SCOPED_TRACE(::testing::Message()
                 << "Tasks length: " << task_titles.size());
    focus_mode_chip_carousel()->SetTasks(GetTasks(task_titles));
    EXPECT_EQ(task_titles.size(),
              GetScrollContents()->GetChildrenInZOrder().size());
    EXPECT_NE(task_titles.empty(), focus_mode_chip_carousel()->HasTasks());

    std::vector<LabelMatcherMatcherP<std::u16string>> task_labels = {};
    for (const std::string& task : task_titles) {
      task_labels.push_back(LabelMatcher(base::UTF8ToUTF16(task)));
    }

    EXPECT_THAT(GetScrollContents()->GetChildrenInZOrder(),
                testing::ElementsAreArray(task_labels));
  };

  validate_tasks({});
  validate_tasks(kTestTaskTitles);
  validate_tasks({"Only one task"});
  validate_tasks({"Maximum", "of", "five", "tasks", "populated"});
}

// Tests that if more than 5 tasks are provided, the carousel only populates the
// first 5.
TEST_F(FocusModeChipCarouselTest, MaxOfFive) {
  focus_mode_chip_carousel()->SetTasks(
      GetTasks({"one", "two", "three", "four", "five", "six"}));
  EXPECT_EQ(5u, GetScrollContents()->GetChildrenInZOrder().size());

  // The first 5 tasks should be populated.
  std::vector<LabelMatcherMatcherP<std::u16string>> task_labels = {};
  for (const std::string& task_title :
       {"one", "two", "three", "four", "five"}) {
    task_labels.push_back(LabelMatcher(base::UTF8ToUTF16(task_title)));
  }
  EXPECT_THAT(GetScrollContents()->GetChildrenInZOrder(),
              testing::ElementsAreArray(task_labels));
}

// Tests that the gradient exists on sides of the scroll that are overflowed,
// and that the overflow buttons exist on those sides when hovered.
TEST_F(FocusModeChipCarouselTest, GradientOnScroll) {
  // The scroll view should be by default empty with no gradient.
  EXPECT_FALSE(GetScrollView()->layer()->HasGradientMask());

  // Setting 1 task shouldn't make the scroll view overflow, so there should
  // still be no gradient.
  focus_mode_chip_carousel()->SetTasks(GetTasks({"Preparing for I485 form"}));
  views::test::RunScheduledLayout(focus_mode_chip_carousel());
  EXPECT_FALSE(GetScrollView()->layer()->HasGradientMask());

  // Three tasks should overflow the scroll view and the gradient should appear.
  focus_mode_chip_carousel()->SetTasks(GetTasks(kTestTaskTitles));
  views::test::RunScheduledLayout(focus_mode_chip_carousel());
  EXPECT_TRUE(GetScrollView()->layer()->HasGradientMask());

  // Neither overflow button should be visible before hovering.
  EXPECT_FALSE(GetLeftOverflowIcon()->GetVisible());
  EXPECT_FALSE(GetRightOverflowIcon()->GetVisible());

  // Hovering should make the right overflow button appear.
  GetEventGenerator()->MoveMouseTo(
      focus_mode_chip_carousel()->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(GetLeftOverflowIcon()->GetVisible());
  EXPECT_TRUE(GetRightOverflowIcon()->GetVisible());

  // Clicking the right overflow button should make both overflow buttons
  // appear, now that the left side should be overflown.
  GetEventGenerator()->MoveMouseTo(
      GetRightOverflowIcon()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(GetLeftOverflowIcon()->GetVisible());
  EXPECT_TRUE(GetRightOverflowIcon()->GetVisible());

  // Both overflow icons should be shown on top of the scroll view.
  EXPECT_EQ(gfx::Size(kWidgetWidth, 32),
            GetScrollView()->GetBoundsInScreen().size());
  EXPECT_EQ(gfx::Size(28, 32),
            GetLeftOverflowIcon()->GetBoundsInScreen().size());
  EXPECT_EQ(gfx::Size(28, 32),
            GetRightOverflowIcon()->GetBoundsInScreen().size());
  EXPECT_TRUE(
      GetLeftOverflowIcon()->HitTestRect(views::View::ConvertRectToTarget(
          GetScrollContents(), GetLeftOverflowIcon(),
          GetScrollView()->GetVisibleRect())));
  EXPECT_TRUE(
      GetRightOverflowIcon()->HitTestRect(views::View::ConvertRectToTarget(
          GetScrollContents(), GetRightOverflowIcon(),
          GetScrollView()->GetVisibleRect())));
}

// Tests that the gradient shows up on the correct side in RTL.
TEST_F(FocusModeChipCarouselTest, GradientInRTL) {
  base::i18n::SetRTLForTesting(true);

  focus_mode_chip_carousel()->SetTasks(GetTasks(kTestTaskTitles));
  views::test::RunScheduledLayout(focus_mode_chip_carousel());
  EXPECT_TRUE(GetScrollView()->layer()->HasGradientMask());

  // In RTL the carousel starts on the right side, so we can only scroll to the
  // left and not to the right. Because of this the gradient should only be
  // shown on the left side.
  ASSERT_EQ(2u, GetScrollView()->layer()->gradient_mask().step_count());
  auto steps = GetScrollView()->layer()->gradient_mask().steps();
  const float allowed_difference = 0.0001f;

  EXPECT_FLOAT_EQ(0.0f, steps.front().fraction);
  EXPECT_EQ(0u, steps.front().alpha);
  EXPECT_NEAR(kGradientWidth / kWidgetWidth, steps[1].fraction,
              allowed_difference);
  EXPECT_EQ(255u, steps[1].alpha);
}

}  // namespace ash
