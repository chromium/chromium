// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/power/adaptive_charging_nudge_controller.h"
#include "ash/system/tray/system_nudge.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

namespace {

// Utility class to wait/observe the closing of the nudge.
class NudgeWidgetObserver : public views::WidgetObserver {
 public:
  NudgeWidgetObserver(views::Widget* widget) {
    if (!widget)
      return;

    widget_observation_.Observe(widget);
  }

  void WaitForClose() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override {
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Enables or disables the user pref for the entire feature.
void SetAdaptiveChargingPref(bool enabled) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kPowerAdaptiveChargingEnabled, enabled);
  base::RunLoop().RunUntilIdle();
}

}  // namespace

class AdaptiveChargingNudgeControllerTest : public AshTestBase {
 public:
  AdaptiveChargingNudgeControllerTest() = default;
  AdaptiveChargingNudgeControllerTest(
      const AdaptiveChargingNudgeControllerTest&) = delete;
  AdaptiveChargingNudgeControllerTest& operator=(
      const AdaptiveChargingNudgeControllerTest&) = delete;
  ~AdaptiveChargingNudgeControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<AdaptiveChargingNudgeController>();
  }

  AdaptiveChargingNudgeController* GetController() { return controller_.get(); }

  void WaitForWidgetClose(AdaptiveChargingNudgeController* controller,
                          SystemNudge* nudge) {
    views::Widget* nudge_widget = nudge->widget();
    ASSERT_TRUE(nudge_widget);
    EXPECT_FALSE(nudge_widget->IsClosed());

    // Slow down the duration of the nudge.
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

    // Pretend the hide nudge timer has elapsed.
    NudgeWidgetObserver widget_close_observer(nudge_widget);
    controller->FireHideNudgeTimerForTesting();

    EXPECT_TRUE(nudge_widget->GetLayer()->GetAnimator()->is_animating());

    widget_close_observer.WaitForClose();
  }

 private:
  std::unique_ptr<AdaptiveChargingNudgeController> controller_;
};

TEST_F(AdaptiveChargingNudgeControllerTest, ShowsAndHidesNudge) {
  AdaptiveChargingNudgeController* controller = GetController();
  ASSERT_TRUE(controller);

  SetAdaptiveChargingPref(true);
  controller->ShowNudge();
  ASSERT_TRUE(controller->GetNudgeDelayTimerForTesting()->IsRunning());
  controller->GetNudgeDelayTimerForTesting()->FireNow();
  SystemNudge* nudge = controller->GetSystemNudgeForTesting();
  ASSERT_TRUE(nudge);

  WaitForWidgetClose(controller, nudge);
}

TEST_F(AdaptiveChargingNudgeControllerTest, NoNudgeShowForDisabledFeature) {
  AdaptiveChargingNudgeController* controller = GetController();
  ASSERT_TRUE(controller);

  SetAdaptiveChargingPref(false);
  controller->ShowNudge();
  ASSERT_FALSE(controller->GetNudgeDelayTimerForTesting());
  SystemNudge* nudge = controller->GetSystemNudgeForTesting();
  ASSERT_FALSE(nudge);
}

TEST_F(AdaptiveChargingNudgeControllerTest, NudgeShowExactlyOnce) {
  AdaptiveChargingNudgeController* controller = GetController();
  ASSERT_TRUE(controller);

  SetAdaptiveChargingPref(true);

  // First time, nudge is shown.
  controller->ShowNudge();
  ASSERT_TRUE(controller->GetNudgeDelayTimerForTesting()->IsRunning());
  controller->GetNudgeDelayTimerForTesting()->FireNow();
  SystemNudge* nudge1 = controller->GetSystemNudgeForTesting();
  ASSERT_TRUE(nudge1);
  WaitForWidgetClose(controller, nudge1);

  // No nudge for the second time.
  controller->ShowNudge();
  ASSERT_FALSE(controller->GetNudgeDelayTimerForTesting()->IsRunning());
  SystemNudge* nudge2 = controller->GetSystemNudgeForTesting();
  ASSERT_FALSE(nudge2);
}

}  // namespace ash
