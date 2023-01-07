// Copyright 2022 The Chromium Authors
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
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

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

  void WaitForWidgetDestruction(AdaptiveChargingNudgeController* controller,
                                SystemNudge* nudge) {
    views::Widget* nudge_widget = nudge->widget();
    ASSERT_TRUE(nudge_widget);
    EXPECT_FALSE(nudge_widget->IsClosed());

    // Slow down the duration of the nudge.
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

    // Pretend the hide nudge timer has elapsed.
    controller->FireHideNudgeTimerForTesting();
    EXPECT_TRUE(nudge_widget->GetLayer()->GetAnimator()->is_animating());

    views::test::WidgetDestroyedWaiter widget_destroyed_waiter(nudge_widget);
    widget_destroyed_waiter.Wait();
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

  WaitForWidgetDestruction(controller, nudge);
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
  WaitForWidgetDestruction(controller, nudge1);

  // No nudge for the second time.
  controller->ShowNudge();
  ASSERT_FALSE(controller->GetNudgeDelayTimerForTesting()->IsRunning());
  SystemNudge* nudge2 = controller->GetSystemNudgeForTesting();
  ASSERT_FALSE(nudge2);
}

}  // namespace ash
