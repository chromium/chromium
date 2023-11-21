// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_chip_carousel.h"

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"

namespace {

MATCHER_P(LabelMatcher, task, "") {
  return static_cast<views::LabelButton*>(arg)->GetText() == task;
}

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

    // Create a container view to stop the carousel from stretching to the
    // widget dimensions.
    container_view_ = widget_->SetContentsView(std::make_unique<views::View>());
    auto focus_mode_chip_carousel =
        std::make_unique<FocusModeChipCarousel>(base::DoNothing());
    focus_mode_chip_carousel_ = focus_mode_chip_carousel.get();
    container_view_->AddChildView(std::move(focus_mode_chip_carousel));
  }

  void TearDown() override {
    focus_mode_chip_carousel_ = nullptr;
    container_view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  FocusModeChipCarousel* focus_mode_chip_carousel() {
    return focus_mode_chip_carousel_;
  }

  views::View* GetTaskScrollContents() {
    return focus_mode_chip_carousel_->contents();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> container_view_;
  raw_ptr<FocusModeChipCarousel> focus_mode_chip_carousel_;
};

// Tests that the task list displays the list of tasks.
TEST_F(FocusModeChipCarouselTest, ChipCarouselPopulates) {
  EXPECT_FALSE(focus_mode_chip_carousel()->HasTasks());
  auto validate_tasks = [&](const std::vector<std::u16string> tasks) {
    SCOPED_TRACE(::testing::Message() << "Tasks length: " << tasks.size());
    focus_mode_chip_carousel()->SetTasks(tasks);

    EXPECT_EQ(tasks.size(),
              GetTaskScrollContents()->GetChildrenInZOrder().size());
    EXPECT_NE(tasks.empty(), focus_mode_chip_carousel()->HasTasks());

    std::vector<LabelMatcherMatcherP<std::u16string>> task_labels = {};
    for (const std::u16string& task : tasks) {
      task_labels.push_back(LabelMatcher(task));
    }
    EXPECT_THAT(GetTaskScrollContents()->GetChildrenInZOrder(),
                testing::ElementsAreArray(task_labels));
  };

  validate_tasks({});
  validate_tasks({u"Preparing for I485 form", u"Podcast interview Script",
                  u"Book a flight to Seoul"});
  validate_tasks({u"Only one task"});
  validate_tasks({u"Maximum", u"of", u"five", u"tasks", u"populated"});
}

// Tests that if more than 5 tasks are provided, the carousel only populates the
// first 5.
TEST_F(FocusModeChipCarouselTest, MaxOfFive) {
  focus_mode_chip_carousel()->SetTasks(
      {u"one", u"two", u"three", u"four", u"five", u"six"});
  EXPECT_EQ(5u, GetTaskScrollContents()->GetChildrenInZOrder().size());

  // The first 5 tasks should be populated.
  std::vector<LabelMatcherMatcherP<std::u16string>> task_labels = {};
  for (const std::u16string& task :
       {u"one", u"two", u"three", u"four", u"five"}) {
    task_labels.push_back(LabelMatcher(task));
  }
  EXPECT_THAT(GetTaskScrollContents()->GetChildrenInZOrder(),
              testing::ElementsAreArray(task_labels));
}

}  // namespace ash
