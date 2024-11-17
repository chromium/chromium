// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item_view_scheduler.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_ui_task_pool.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/window_util.h"
#include "base/functional/callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class OverviewItemViewSchedulerTest : public AshTestBase {
 protected:
  OverviewItemViewSchedulerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    overview_item_window_ = CreateTestWindow();
    ASSERT_FALSE(window_util::IsMinimizedOrTucked(overview_item_window_.get()));
    overview_item_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    overview_item_widget_->SetOpacity(0.f);
    enter_animation_task_pool_ = std::make_unique<OverviewUiTaskPool>(
        ash_test_helper()->GetHost()->compositor(),
        /*initial_blackout_period=*/base::Milliseconds(100));
  }

  void TearDown() override {
    frame_request_timer_.Stop();
    enter_animation_task_pool_.reset();
    overview_item_widget_.reset();
    overview_item_window_.reset();
    AshTestBase::TearDown();
  }

  void ScheduleOverviewItemViewInitializationForTest(
      base::OnceClosure initialize_cb,
      bool should_enter_without_animations = false) {
    if (!should_enter_without_animations) {
      SimulateOverviewEnterAnimation();
    }
    ScheduleOverviewItemViewInitialization(
        *overview_item_window_, *overview_item_widget_,
        *enter_animation_task_pool_, should_enter_without_animations,
        std::move(initialize_cb));
  }

  void SimulateOverviewEnterAnimation() {
    const base::TimeDelta frame_interval =
        GetContextFactory()->GetDisplayVSyncTimeInterval(
            ash_test_helper()->GetHost()->compositor());
    frame_request_timer_.Start(
        FROM_HERE, frame_interval, this,
        &OverviewItemViewSchedulerTest::SchdeduleNewFrame);
  }

  void SchdeduleNewFrame() {
    ash_test_helper()->GetHost()->compositor()->ScheduleFullRedraw();
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kOverviewSessionInitOptimizations};
  std::unique_ptr<aura::Window> overview_item_window_;
  std::unique_ptr<views::Widget> overview_item_widget_;
  std::unique_ptr<OverviewUiTaskPool> enter_animation_task_pool_;
  base::RepeatingTimer frame_request_timer_;
};

TEST_F(OverviewItemViewSchedulerTest, RunsInitCallbackViaTaskPool) {
  base::test::TestFuture<void> init_signal;
  ScheduleOverviewItemViewInitializationForTest(init_signal.GetCallback());
  ASSERT_FALSE(init_signal.IsReady());
  EXPECT_TRUE(init_signal.Wait());
}

TEST_F(OverviewItemViewSchedulerTest,
       RunsInitCallbackImmediatelyForMinimizedWindow) {
  window_util::MinimizeAndHideWithoutAnimation({overview_item_window_.get()});
  base::test::TestFuture<void> init_signal;
  ScheduleOverviewItemViewInitializationForTest(init_signal.GetCallback());
  EXPECT_TRUE(init_signal.IsReady());
}

TEST_F(OverviewItemViewSchedulerTest,
       RunsInitCallbackImmediatelyIfNoEnterAnimation) {
  base::test::TestFuture<void> init_signal;
  ScheduleOverviewItemViewInitializationForTest(init_signal.GetCallback(),
                                                true);
  EXPECT_TRUE(init_signal.IsReady());
}

TEST_F(OverviewItemViewSchedulerTest, RunsInitCallbackAfterWindowIsOpaque) {
  base::test::TestFuture<void> init_signal;
  ScheduleOverviewItemViewInitializationForTest(init_signal.GetCallback());
  ASSERT_FALSE(init_signal.IsReady());
  overview_item_widget_->SetOpacity(1.f);
  EXPECT_TRUE(init_signal.IsReady());
}

TEST_F(OverviewItemViewSchedulerTest, RunsInitCallbackAfterWindowIsFadedIn) {
  base::test::TestFuture<void> init_signal;
  ScheduleOverviewItemViewInitializationForTest(init_signal.GetCallback());
  ASSERT_FALSE(init_signal.IsReady());
  FadeInWidgetToOverview(overview_item_widget_.get(),
                         OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
                         /*observe=*/false);
  EXPECT_TRUE(init_signal.IsReady());
}

}  // namespace
}  // namespace ash
