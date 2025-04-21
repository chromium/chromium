// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button_tap_overlay.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/scanner/scanner_enterprise_policy.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr base::TimeDelta kAnimationDelay = base::Milliseconds(200);
constexpr char kOverlayClassName[] = "HomeButtonTapOverlay";

enum TestVariant { kClamshell, kTablet, kTabletWithBackButton };

class HomeButtonTapOverlayTest
    : public AshTestBase,
      public testing::WithParamInterface<TestVariant> {
 public:
  HomeButtonTapOverlayTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    // In Tablet mode, home button is shown if a board has kAshEnableTabletMode
    // and kAccessibilityTabletModeShelfNavigationButtonsEnabled is true.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTabletMode);

    AshTestBase::SetUp();

    if (ash::assistant::features::IsNewEntryPointEnabled()) {
      GTEST_SKIP()
          << "Assistant is not available if new entry point is enabled. "
             "crbug.com/388361414";
    }

    // Enable Assistant
    Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
        assistant::prefs::kAssistantEnabled, true);
    AssistantState* assistant_state = AssistantState::Get();
    assistant_state->NotifyFeatureAllowed(
        assistant::AssistantAllowedState::ALLOWED);
    assistant_state->NotifyStatusChanged(assistant::AssistantStatus::READY);

    PrefService* prefs =
        Shell::Get()->session_controller()->GetActivePrefService();
    // Disable Sunfish and Scanner via enterprise policies.
    auto* capture_mode_controller = CaptureModeController::Get();
    auto* test_capture_mode_delegate = static_cast<TestCaptureModeDelegate*>(
        capture_mode_controller->delegate_for_testing());
    test_capture_mode_delegate->set_is_search_allowed_by_policy(false);
    prefs->SetInteger(prefs::kScannerEnterprisePolicyAllowed,
                      static_cast<int>(ScannerEnterprisePolicy::kDisallowed));
    ASSERT_FALSE(CanShowSunfishOrScannerUi());

    const TestVariant test_variant = GetParam();
    switch (test_variant) {
      case kClamshell:
        ASSERT_FALSE(Shell::Get()->IsInTabletMode());
        break;
      case kTablet:
        Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
            prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, true);
        Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
        ASSERT_FALSE(ShelfConfig::Get()->is_in_app());
        break;
      case kTabletWithBackButton:
        Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
            prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, true);
        Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
        // A back button is shown if there is an active window.
        CreateTestWindow();
        ActivateTestWindow();
        ASSERT_TRUE(ShelfConfig::Get()->is_in_app());
        break;
    }
  }

  void TearDown() override {
    window_.reset();

    AshTestBase::TearDown();
  }

 protected:
  const views::View* GetTapOverlay() {
    for (const views::View* child : home_button()->children()) {
      if (child->GetClassName() == kOverlayClassName) {
        return child;
      }
    }

    return nullptr;
  }

  HomeButton* home_button() {
    return GetPrimaryShelf()
        ->shelf_widget()
        ->navigation_widget()
        ->GetHomeButton();
  }

  std::vector<gfx::Rect> TakeClipRectSnapshot(const ui::Layer* layer) {
    std::vector<gfx::Rect> clip_rects;
    while (layer) {
      clip_rects.push_back(layer->clip_rect());
      layer = layer->parent();
    }
    return clip_rects;
  }

  void SendGestureEventToHomeButton(ui::EventType type) {
    ui::GestureEvent event(0, 0, ui::EF_NONE, base::TimeTicks(),
                           ui::GestureEventDetails(type));
    home_button()->OnGestureEvent(&event);
  }

  void ActivateTestWindow() { wm::ActivateWindow(window_.get()); }

 private:
  void CreateTestWindow() {
    window_ = AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

  std::unique_ptr<aura::Window> window_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HomeButtonTapOverlayTest,
                         testing::Values(TestVariant::kClamshell,
                                         TestVariant::kTablet,
                                         TestVariant::kTabletWithBackButton));

// HomeButtonTapOverlay renders burst animation which goes outside of its view
// size. Make sure that it's not clipped by an ancestor layer.
//
// For long press, events and expectations are follows:
// - EventType::kGestureTapDown: Ripple animation starts
// - EventType::kGestureLongPress: Burst animation starts (clip rect will be
// restored
//   after the animation)
// - EventType::kGestureTapCancel: Nothing should happen
// - EventType::kGestureLongTap: Nothing should happen
TEST_P(HomeButtonTapOverlayTest, BurstAnimationWithLongPress) {
  const views::View* tap_overlay = GetTapOverlay();
  ASSERT_THAT(tap_overlay, testing::NotNull());

  const ui::Layer* tap_overlay_layer = tap_overlay->layer();

  std::vector<gfx::Rect> clip_rect_snapshot_before =
      TakeClipRectSnapshot(tap_overlay_layer);

  SendGestureEventToHomeButton(ui::EventType::kGestureTapDown);

  // HomeButtonController delays the animation for kAnimationDelay.
  task_environment()->FastForwardBy(kAnimationDelay);

  // Confirm that no clip rect is set in ancestor layers.
  std::vector<gfx::Rect> clip_rects_during_animation =
      TakeClipRectSnapshot(tap_overlay_layer);
  for (const gfx::Rect& clip_rect : clip_rects_during_animation) {
    EXPECT_TRUE(clip_rect.IsEmpty());
  }

  // HomeButtonTapOverlay starts burst animation with
  // EventType::kGestureLongPress.
  SendGestureEventToHomeButton(ui::EventType::kGestureLongPress);

  // Burst animation ends immediately in this test case. Confirm that clip rect
  // is restored now.
  EXPECT_EQ(TakeClipRectSnapshot(tap_overlay_layer), clip_rect_snapshot_before);

  SendGestureEventToHomeButton(ui::EventType::kGestureTapCancel);
  SendGestureEventToHomeButton(ui::EventType::kGestureLongTap);
  // Confirm that nothing should happen with the following TAP_CANCEL and
  // LONG_TAP events.
  EXPECT_EQ(TakeClipRectSnapshot(tap_overlay_layer), clip_rect_snapshot_before);
}

// HomeButtonTapOverlay renders a ripple animation with a tap, which goes beyond
// the size of home button.
TEST_P(HomeButtonTapOverlayTest, RippleAnimationWithTap) {
  const views::View* tap_overlay = GetTapOverlay();
  ASSERT_THAT(tap_overlay, testing::NotNull());

  const ui::Layer* tap_overlay_layer = tap_overlay->layer();

  std::vector<gfx::Rect> clip_rect_snapshot_before =
      TakeClipRectSnapshot(tap_overlay_layer);

  SendGestureEventToHomeButton(ui::EventType::kGestureTapDown);

  // HomeButtonController delays assistant animation for
  // kAssistantAnimationDelay.
  task_environment()->FastForwardBy(kAnimationDelay);

  // Confirm that no clip rect is set in ancestor layers.
  std::vector<gfx::Rect> clip_rects_during_animation =
      TakeClipRectSnapshot(tap_overlay_layer);
  for (const gfx::Rect& clip_rect : clip_rects_during_animation) {
    EXPECT_TRUE(clip_rect.IsEmpty());
  }

  SendGestureEventToHomeButton(ui::EventType::kGestureTap);

  // The above tap will de-activate test window and hides back button.
  // Re-activate the window to show a back button. Clip rect can be different if
  // no back button is shown.
  if (GetParam() == kTabletWithBackButton) {
    ActivateTestWindow();
  }

  EXPECT_EQ(TakeClipRectSnapshot(tap_overlay_layer), clip_rect_snapshot_before);
}

// HomeButtonTapOverlay renders a ripple animation with a tap, which goes beyond
// the size of home button.
TEST_P(HomeButtonTapOverlayTest,
       RippleAnimationWithAssistantDisabledDuringTap) {
  const views::View* tap_overlay = GetTapOverlay();
  ASSERT_THAT(tap_overlay, testing::NotNull());

  const ui::Layer* tap_overlay_layer = tap_overlay->layer();

  std::vector<gfx::Rect> clip_rect_snapshot_before =
      TakeClipRectSnapshot(tap_overlay_layer);

  SendGestureEventToHomeButton(ui::EventType::kGestureTapDown);

  // HomeButtonController delays assistant animation for
  // kAssistantAnimationDelay.
  task_environment()->FastForwardBy(kAnimationDelay);

  // Confirm that no clip rect is set in ancestor layers.
  std::vector<gfx::Rect> clip_rects_during_animation =
      TakeClipRectSnapshot(tap_overlay_layer);
  for (const gfx::Rect& clip_rect : clip_rects_during_animation) {
    EXPECT_TRUE(clip_rect.IsEmpty());
  }

  // Assistant turns disabled during the tap (due to policy, for example).
  AssistantState* assistant_state = AssistantState::Get();
  assistant_state->NotifyFeatureAllowed(
      assistant::AssistantAllowedState::DISALLOWED_BY_POLICY);

  SendGestureEventToHomeButton(ui::EventType::kGestureTap);

  // The above tap will de-activate test window and hides back button.
  // Re-activate the window to show a back button. Clip rect can be different if
  // no back button is shown.
  if (GetParam() == kTabletWithBackButton) {
    ActivateTestWindow();
  }

  EXPECT_EQ(TakeClipRectSnapshot(tap_overlay_layer), clip_rect_snapshot_before);
}

}  // namespace
}  // namespace ash
