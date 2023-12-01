// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_chip_carousel.h"

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace {

MATCHER_P(LabelMatcher, task, "") {
  return static_cast<views::LabelButton*>(arg)->GetText() == task;
}

std::vector<std::u16string> kTestTaskTitles = {u"Preparing for I485 form",
                                               u"Podcast interview Script",
                                               u"Book a flight to Seoul"};

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
    widget_->SetBounds(gfx::Rect(/*width=*/320, /*height=*/48));

    focus_mode_chip_carousel_ = widget_->SetContentsView(
        std::make_unique<FocusModeChipCarousel>(base::DoNothing()));
  }

  void TearDown() override {
    focus_mode_chip_carousel_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  FocusModeChipCarousel* focus_mode_chip_carousel() {
    return focus_mode_chip_carousel_;
  }

  views::ScrollView* GetScrollView() {
    return focus_mode_chip_carousel_->scroll_view_;
  }

  views::View* GetScrollContents() {
    return focus_mode_chip_carousel_->scroll_view_->contents();
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
  auto validate_tasks = [&](const std::vector<std::u16string> tasks) {
    SCOPED_TRACE(::testing::Message() << "Tasks length: " << tasks.size());
    focus_mode_chip_carousel()->SetTasks(tasks);
    std::vector<views::View*> task_chips =
        GetScrollContents()->GetChildrenInZOrder();

    EXPECT_EQ(tasks.size(), task_chips.size());
    EXPECT_NE(tasks.empty(), focus_mode_chip_carousel()->HasTasks());

    std::vector<LabelMatcherMatcherP<std::u16string>> task_labels = {};
    for (const std::u16string& task : tasks) {
      task_labels.push_back(LabelMatcher(task));
    }

    EXPECT_THAT(task_chips, testing::ElementsAreArray(task_labels));
  };

  validate_tasks({});
  validate_tasks(kTestTaskTitles);
  validate_tasks({u"Only one task"});
  validate_tasks({u"Maximum", u"of", u"five", u"tasks", u"populated"});
}

// Tests that if more than 5 tasks are provided, the carousel only populates the
// first 5.
TEST_F(FocusModeChipCarouselTest, MaxOfFive) {
  focus_mode_chip_carousel()->SetTasks(
      {u"one", u"two", u"three", u"four", u"five", u"six"});

  std::vector<views::View*> task_chips =
      GetScrollContents()->GetChildrenInZOrder();
  EXPECT_EQ(5u, task_chips.size());

  // The first 5 tasks should be populated.
  std::vector<LabelMatcherMatcherP<std::u16string>> task_labels = {};
  for (const std::u16string& task :
       {u"one", u"two", u"three", u"four", u"five"}) {
    task_labels.push_back(LabelMatcher(task));
  }
  EXPECT_THAT(task_chips, testing::ElementsAreArray(task_labels));
}

// Tests that the gradient exists on sides of the scroll that are overflowed,
// and that the overflow buttons exist on those sides when hovered.
TEST_F(FocusModeChipCarouselTest, GradientOnScroll) {
  // The scroll view should be by default empty with no gradient.
  EXPECT_FALSE(GetScrollView()->layer()->HasGradientMask());

  // Setting 1 task shouldn't make the scroll view overflow, so there should
  // still be no gradient.
  focus_mode_chip_carousel()->SetTasks({u"Preparing for I485 form"});
  views::test::RunScheduledLayout(focus_mode_chip_carousel());
  EXPECT_FALSE(GetScrollView()->layer()->HasGradientMask());

  // Three tasks should overflow the scroll view and the gradient should appear.
  focus_mode_chip_carousel()->SetTasks(kTestTaskTitles);
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
  EXPECT_EQ(gfx::Size(320, 32), GetScrollView()->GetBoundsInScreen().size());
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

}  // namespace ash
