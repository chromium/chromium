// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/haptics_util.h"

#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/gestures/wm_gesture_handler.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/containers/flat_map.h"
#include "ui/aura/window.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

using ui::HapticTouchpadEffect;
using ui::HapticTouchpadEffectStrength;

class InputControllerForTesting : public ui::InputController {
 public:
  InputControllerForTesting() = default;
  ~InputControllerForTesting() override = default;
  InputControllerForTesting(const InputControllerForTesting&) = delete;
  InputControllerForTesting& operator=(const InputControllerForTesting&) =
      delete;

  // ui::InputController:
  bool HasMouse() override { return false; }
  bool HasPointingStick() override { return false; }
  bool HasTouchpad() override { return false; }
  bool HasHapticTouchpad() override { return true; }
  bool IsCapsLockEnabled() override { return false; }
  void SetCapsLockEnabled(bool enabled) override {}
  void SetNumLockEnabled(bool enabled) override {}
  bool IsAutoRepeatEnabled() override { return true; }
  void SetAutoRepeatEnabled(bool enabled) override {}
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override {}
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override {}
  void SetCurrentLayoutByName(const std::string& layout_name) override {}
  void SetTouchpadSensitivity(int value) override {}
  void SetTouchpadScrollSensitivity(int value) override {}
  void SetTapToClick(bool enabled) override {}
  void SetThreeFingerClick(bool enabled) override {}
  void SetTapDragging(bool enabled) override {}
  void SetNaturalScroll(bool enabled) override {}
  void SetTouchpadAcceleration(bool enabled) override {}
  void SetTouchpadScrollAcceleration(bool enabled) override {}
  void SetTouchpadHapticFeedback(bool enabled) override {}
  void SetTouchpadHapticClickSensitivity(int value) override {}
  void SetMouseSensitivity(int value) override {}
  void SetMouseScrollSensitivity(int value) override {}
  void SetPrimaryButtonRight(bool right) override {}
  void SetMouseReverseScroll(bool enabled) override {}
  void SetMouseAcceleration(bool enabled) override {}
  void SuspendMouseAcceleration() override {}
  void EndMouseAccelerationSuspension() override {}
  void SetMouseScrollAcceleration(bool enabled) override {}
  void SetPointingStickSensitivity(int value) override {}
  void SetPointingStickPrimaryButtonRight(bool right) override {}
  void SetPointingStickAcceleration(bool enabled) override {}
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override {
    std::move(reply).Run(std::string());
  }
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override {
    std::move(reply).Run(std::vector<base::FilePath>());
  }
  void SetTouchEventLoggingEnabled(bool enabled) override {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  void SetTapToClickPaused(bool state) override {}
  void SetInternalTouchpadEnabled(bool enabled) override {}
  bool IsInternalTouchpadEnabled() const override { return false; }
  void SetTouchscreensEnabled(bool enabled) override {}
  void GetStylusSwitchState(GetStylusSwitchStateReply reply) override {
    // Return that there is no stylus in the garage; this test class
    // does not need to trigger stylus charging behaviours.
    std::move(reply).Run(ui::StylusState::REMOVED);
  }
  void PlayVibrationEffect(int id,
                           uint8_t amplitude,
                           uint16_t duration_millis) override {}
  void StopVibration(int id) override {}
  void PlayHapticTouchpadEffect(
      HapticTouchpadEffect effect,
      HapticTouchpadEffectStrength strength) override {
    send_haptic_count_[effect][strength]++;
  }
  void SetHapticTouchpadEffectForNextButtonRelease(
      HapticTouchpadEffect effect,
      HapticTouchpadEffectStrength strength) override {}
  void SetInternalKeyboardFilter(
      bool enable_filter,
      std::vector<ui::DomCode> allowed_keys) override {}
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) override {}

  // Return haptic count for effect/strength combination for testing.
  int GetSendHapticCount(HapticTouchpadEffect effect,
                         HapticTouchpadEffectStrength strength) {
    return send_haptic_count_[effect][strength];
  }

 private:
  // A map of map that stores counts for given haptic effect/strength.
  // This is used for testing only.
  base::flat_map<HapticTouchpadEffect,
                 base::flat_map<HapticTouchpadEffectStrength, int>>
      send_haptic_count_;
};

using HapticsUtilTest = AshTestBase;

// Test haptic feedback with all effect/strength combination.
TEST_F(HapticsUtilTest, HapticFeedbackBasic) {
  auto input_controller = std::make_unique<InputControllerForTesting>();
  haptics_util::SetInputControllerForTesting(input_controller.get());

  std::vector<HapticTouchpadEffect> effects = {
      HapticTouchpadEffect::kSnap,    HapticTouchpadEffect::kKnock,
      HapticTouchpadEffect::kTick,    HapticTouchpadEffect::kPress,
      HapticTouchpadEffect::kRelease,
  };
  std::vector<HapticTouchpadEffectStrength> strengths = {
      HapticTouchpadEffectStrength::kLow,
      HapticTouchpadEffectStrength::kMedium,
      HapticTouchpadEffectStrength::kHigh,
  };

  for (HapticTouchpadEffect effect : effects) {
    for (HapticTouchpadEffectStrength strength : strengths) {
      for (int count = 0; count < 16; count++) {
        haptics_util::PlayHapticTouchpadEffect(effect, strength);
        EXPECT_EQ(count + 1,
                  input_controller->GetSendHapticCount(effect, strength));
      }
    }
  }
}

// Test haptic feedback for nornal window snapping. This covers drag to snap
// primary/secondary/maximize.
TEST_F(HapticsUtilTest, HapticFeedbackForNormalWindowSnap) {
  auto input_controller = std::make_unique<InputControllerForTesting>();
  haptics_util::SetInputControllerForTesting(input_controller.get());

  UpdateDisplay("800x600");
  gfx::Rect bounds(200, 200, 300, 300);
  std::unique_ptr<aura::Window> window = CreateTestWindow(bounds);
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Each element in the vector represents a test case. The first in the pair is
  // the drag target point, which is used to trigger window snapping, and the
  // second is the expected window bounds after drag.
  std::vector<std::pair<gfx::Point, gfx::Rect>> test_cases = {
      {{0, 220}, {0, 0, 400, 552}},
      {{800, 220}, {400, 0, 400, 552}},
      {{350, 0}, {0, 0, 800, 552}},
  };

  // Drag by touch should not trigger haptic feedback.
  for (size_t i = 0; i < test_cases.size(); i++) {
    std::pair<gfx::Point, gfx::Rect> test_case = test_cases[i];
    window->SetBounds(bounds);
    gfx::Point start =
        window->GetBoundsInScreen().top_center() + gfx::Vector2d(0, 20);
    event_generator->set_current_screen_location(start);
    event_generator->PressTouch();
    event_generator->MoveTouch(test_case.first);
    EXPECT_EQ(0, input_controller->GetSendHapticCount(
                     HapticTouchpadEffect::kSnap,
                     HapticTouchpadEffectStrength::kMedium));
    event_generator->ReleaseTouch();
    EXPECT_EQ(test_case.second, window->GetBoundsInScreen());
  }

  // Drag by touchpad/mouse should trigger haptic feedback.
  for (size_t i = 0; i < test_cases.size(); i++) {
    std::pair<gfx::Point, gfx::Rect> test_case = test_cases[i];
    window->SetBounds(bounds);
    gfx::Point start =
        window->GetBoundsInScreen().top_center() + gfx::Vector2d(0, 20);
    event_generator->set_current_screen_location(start);
    event_generator->PressLeftButton();
    event_generator->MoveMouseTo(test_case.first);
    WorkspaceWindowResizer* workspace_resizer =
        WorkspaceWindowResizer::GetInstanceForTest();
    if (workspace_resizer->dwell_countdown_timer_.IsRunning())
      workspace_resizer->dwell_countdown_timer_.FireNow();
    EXPECT_EQ((int)i + 1, input_controller->GetSendHapticCount(
                              HapticTouchpadEffect::kSnap,
                              HapticTouchpadEffectStrength::kMedium));
    event_generator->ReleaseLeftButton();
    EXPECT_EQ(test_case.second, window->GetBoundsInScreen());
  }
}

// Test haptic feedback for overview window snapping. This covers drag to split
// in overview.
TEST_F(HapticsUtilTest, HapticFeedbackForOverviewWindowSnap) {
  auto input_controller = std::make_unique<InputControllerForTesting>();
  haptics_util::SetInputControllerForTesting(input_controller.get());
  OverviewController* overview_controller = Shell::Get()->overview_controller();

  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(200, 200, 300, 300));
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Each element in the vector represents a test case. The first in the pair is
  // the drag target point, which is used to trigger window snapping, and the
  // second is the expected window bounds after drag.
  std::vector<std::pair<gfx::Point, gfx::Rect>> test_cases = {
      {{0, 300}, {0, 0, 400, 552}},
      {{800, 300}, {400, 0, 400, 552}},
  };

  // Drag by touch should not trigger haptic feedback.
  for (size_t i = 0; i < test_cases.size(); i++) {
    std::pair<gfx::Point, gfx::Rect> test_case = test_cases[i];
    EnterOverview();
    OverviewItem* overview_item =
        overview_controller->overview_session()->GetOverviewItemForWindow(
            window.get());

    event_generator->set_current_screen_location(
        gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
    event_generator->PressTouch();
    event_generator->MoveTouch(test_case.first);
    EXPECT_TRUE(overview_controller->InOverviewSession());
    EXPECT_EQ(0, input_controller->GetSendHapticCount(
                     HapticTouchpadEffect::kSnap,
                     HapticTouchpadEffectStrength::kMedium));
    event_generator->ReleaseTouch();
    EXPECT_FALSE(overview_controller->InOverviewSession());
    EXPECT_EQ(test_case.second, window->GetBoundsInScreen());
  }

  // Drag by touchpad/mouse should trigger haptic feedback.
  for (size_t i = 0; i < test_cases.size(); i++) {
    std::pair<gfx::Point, gfx::Rect> test_case = test_cases[i];
    EnterOverview();
    OverviewItem* overview_item =
        overview_controller->overview_session()->GetOverviewItemForWindow(
            window.get());

    event_generator->set_current_screen_location(
        gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
    event_generator->PressLeftButton();
    event_generator->MoveMouseTo(test_case.first);
    EXPECT_TRUE(overview_controller->InOverviewSession());
    EXPECT_EQ((int)i + 1, input_controller->GetSendHapticCount(
                              HapticTouchpadEffect::kSnap,
                              HapticTouchpadEffectStrength::kMedium));
    event_generator->ReleaseLeftButton();
    EXPECT_FALSE(overview_controller->InOverviewSession());
    EXPECT_EQ(test_case.second, window->GetBoundsInScreen());
  }
}

// Test haptic feedback for off limits desk switching, e.g. swiping left from
// the first desk and swiping right from the last desk.
TEST_F(HapticsUtilTest, HapticFeedbackForDeskSwitchingOffLimits) {
  auto* desk_controller = DesksController::Get();
  auto input_controller = std::make_unique<InputControllerForTesting>();
  haptics_util::SetInputControllerForTesting(input_controller.get());

  // Make sure to start with two desks.
  NewDesk();
  EXPECT_EQ(2u, desk_controller->desks().size());
  EXPECT_EQ(0, desk_controller->GetActiveDeskIndex());
  EXPECT_EQ(
      input_controller->GetSendHapticCount(
          HapticTouchpadEffect::kKnock, HapticTouchpadEffectStrength::kMedium),
      0);

  // Swipe from `desk 0` to `desk 1` should not trigger `kKnock` effect.
  ScrollToSwitchDesks(/*scroll_left=*/false, GetEventGenerator());
  EXPECT_EQ(1, desk_controller->GetActiveDeskIndex());
  EXPECT_EQ(0, input_controller->GetSendHapticCount(
                   HapticTouchpadEffect::kKnock,
                   HapticTouchpadEffectStrength::kMedium));

  // Swipe from `desk 1` to right should trigger `kKnock` effect.
  ScrollToSwitchDesks(/*scroll_left=*/false,
                      /*event_generator=*/GetEventGenerator());
  EXPECT_EQ(1, desk_controller->GetActiveDeskIndex());
  EXPECT_EQ(1, input_controller->GetSendHapticCount(
                   HapticTouchpadEffect::kKnock,
                   HapticTouchpadEffectStrength::kMedium));
}

}  // namespace ash
