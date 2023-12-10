// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_chip_carousel.h"

#include "ash/api/tasks/tasks_types.h"
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

std::vector<std::string> kTestTaskTitles = {"Preparing for I485 form",
                                            "Podcast interview Script",
                                            "Book a flight to Seoul"};

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

  std::unique_ptr<api::Task> MakeTask(const std::string& title) {
    return std::make_unique<api::Task>(
        /*id=*/base::NumberToString(task_id_++), title, /*completed=*/false,
        /*due=*/absl::nullopt, /*has_subtasks=*/false, /*has_email_link=*/false,
        /*has_notes=*/false, /*updated=*/base::Time::Now());
  }

  std::vector<std::unique_ptr<const api::Task>> MakeTasks(
      const std::vector<std::string>& titles) {
    std::vector<std::unique_ptr<const api::Task>> tasks;
    for (const std::string& title : titles) {
      tasks.push_back(MakeTask(title));
    }
    return tasks;
  }

  std::vector<const api::Task*> GetTaskPtrs(
      const std::vector<std::unique_ptr<const api::Task>>& tasks) {
    std::vector<const api::Task*> task_ptrs;
    for (const auto& task : tasks) {
      task_ptrs.push_back(task.get());
    }
    return task_ptrs;
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
  // ID counter for creating fake tasks.
  int task_id_ = 0;
};

// Tests that the task list displays the list of tasks.
TEST_F(FocusModeChipCarouselTest, ChipCarouselPopulates) {
  EXPECT_FALSE(focus_mode_chip_carousel()->HasTasks());
  auto validate_tasks = [&](const std::vector<std::string> task_titles) {
    SCOPED_TRACE(::testing::Message()
                 << "Tasks length: " << task_titles.size());
    auto tasks = MakeTasks(task_titles);
    focus_mode_chip_carousel()->SetTasks(GetTaskPtrs(tasks));

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
  auto tasks = MakeTasks({"one", "two", "three", "four", "five", "six"});
  focus_mode_chip_carousel()->SetTasks(GetTaskPtrs(tasks));
  EXPECT_EQ(5u, GetScrollContents()->GetChildrenInZOrder().size());

  // The first 5 tasks should be populated.
  std::vector<LabelMatcherMatcherP<std::u16string>> task_labels = {};
  for (const std::string& task : {"one", "two", "three", "four", "five"}) {
    task_labels.push_back(LabelMatcher(base::UTF8ToUTF16(task)));
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
  auto tasks_1 = MakeTasks({"Preparing for I485 form"});
  focus_mode_chip_carousel()->SetTasks(GetTaskPtrs(tasks_1));
  views::test::RunScheduledLayout(focus_mode_chip_carousel());
  EXPECT_FALSE(GetScrollView()->layer()->HasGradientMask());

  // Three tasks should overflow the scroll view and the gradient should appear.
  auto tasks_2 = MakeTasks(kTestTaskTitles);
  focus_mode_chip_carousel()->SetTasks(GetTaskPtrs(tasks_2));
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
