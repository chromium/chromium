// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/slider.h"

namespace ash {

namespace {

constexpr float kMinBrightnessLevel = 0.05;

}  // namespace

class UnifiedBrightnessViewTest : public AshTestBase {
 public:
  UnifiedBrightnessViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  UnifiedBrightnessViewTest(const UnifiedBrightnessViewTest&) = delete;
  UnifiedBrightnessViewTest& operator=(const UnifiedBrightnessViewTest&) =
      delete;
  ~UnifiedBrightnessViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    brightness_slider_controller_ =
        controller()->brightness_slider_controller_.get();
    unified_brightness_view_ = static_cast<UnifiedBrightnessView*>(
        controller()->unified_brightness_view_);
    brightness_slider_ =
        brightness_slider_controller_->CreateBrightnessSlider();
  }

  void TearDown() override {
    // Tests the environment tears down with the bubble closed.
    // In `UnifiedVolumeViewTest`, the environment is torn down with the bubble
    // open, so we can test both cases.
    brightness_slider_ = nullptr;
    GetPrimaryUnifiedSystemTray()->CloseBubble();
    AshTestBase::TearDown();
  }

  void WaitUntilUpdated() {
    task_environment()->FastForwardBy(base::Milliseconds(100));
    base::RunLoop().RunUntilIdle();
  }

  UnifiedBrightnessSliderController* brightness_slider_controller() {
    return brightness_slider_controller_;
  }

  UnifiedBrightnessView* unified_brightness_view() {
    return unified_brightness_view_;
  }

  UnifiedBrightnessView* brightness_slider() {
    return brightness_slider_.get();
  }

  views::Slider* slider() { return unified_brightness_view_->slider(); }

  views::Button* more_button() {
    return static_cast<UnifiedBrightnessView*>(
               controller()->unified_brightness_view_)
        ->more_button();
  }

  views::Button* night_light_button() {
    return static_cast<UnifiedBrightnessView*>(
               controller()->unified_brightness_view_)
        ->night_light_button();
  }

  const gfx::VectorIcon& GetIcon(float level) {
    return unified_brightness_view_->GetBrightnessIconForLevel(level);
  }

  UnifiedSystemTrayController* controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

 private:
  // The `UnifiedBrightnessView` containing a `QuickSettingsSlider`, a
  // `NightLight` button, and a drill-in button.
  raw_ptr<UnifiedBrightnessView, DanglingUntriaged> unified_brightness_view_ =
      nullptr;

  // The `UnifiedBrightnessView` containing only a `QuickSettingsSlider`.
  std::unique_ptr<UnifiedBrightnessView> brightness_slider_ = nullptr;

  raw_ptr<UnifiedBrightnessSliderController, DanglingUntriaged>
      brightness_slider_controller_ = nullptr;
};

// Tests to ensure that the `slider_button` does not handle any events,
// letting them get through to the slider. Effectively the `slider_button` is
// part of the slider in the brightness view.
TEST_F(UnifiedBrightnessViewTest, SliderButtonClickThrough) {
  slider()->SetValue(1.0);
  EXPECT_FLOAT_EQ(unified_brightness_view()->slider()->GetValue(), 1.0);

  // A click on the `slider_button` for `unified_brightness_view()` should go
  // through to the slider and change the value to the minimum.
  LeftClickOn(unified_brightness_view()->slider_button());
  EXPECT_FLOAT_EQ(unified_brightness_view()->slider()->GetValue(), 0.0);
}

// Tests that `UnifiedBrightnessView` is made up of a `QuickSettingsSlider`, a
// `NightLight` button, and a drill-in button that leads to the display subpage.
TEST_F(UnifiedBrightnessViewTest, SliderButtonComponents) {
  EXPECT_EQ(unified_brightness_view()->children().size(), 3u);
  EXPECT_STREQ(unified_brightness_view()->children()[0]->GetClassName(),
               "QuickSettingsSlider");

  // TODO(b/257151067): Updates the a11y name id and tooltip text.
  auto* night_light_button =
      static_cast<IconButton*>(unified_brightness_view()->children()[1]);
  EXPECT_STREQ(night_light_button->GetClassName(), "IconButton");
  EXPECT_EQ(night_light_button->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_DISABLED_STATE_TOOLTIP)));
  EXPECT_EQ(night_light_button->GetTooltipText(),
            u"Toggle Night Light. Night Light is off.");

  auto* display_subpage_drill_in_button =
      static_cast<IconButton*>(unified_brightness_view()->children()[2]);
  EXPECT_STREQ(display_subpage_drill_in_button->GetClassName(), "IconButton");
  EXPECT_EQ(
      display_subpage_drill_in_button->GetViewAccessibility().GetCachedName(),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_SETTINGS_TOOLTIP));
  EXPECT_EQ(display_subpage_drill_in_button->GetTooltipText(),
            u"Show display settings");

  // Clicks on the drill-in button and checks `DisplayDetailedView` is shown.
  EXPECT_FALSE(controller()->IsDetailedViewShown());
  LeftClickOn(unified_brightness_view()->children()[2]);
  EXPECT_TRUE(controller()->showing_display_detailed_view());
}

// Tests that `UnifiedBrightnessView` in the display subpage is made up of a
// `QuickSettingsSlider`.
TEST_F(UnifiedBrightnessViewTest, SliderComponent) {
  EXPECT_EQ(brightness_slider()->children().size(), 1u);
  EXPECT_STREQ(brightness_slider()->children()[0]->GetClassName(),
               "QuickSettingsSlider");
}

// Tests the slider icon matches the slider level.
TEST_F(UnifiedBrightnessViewTest, SliderIcon) {
  const float levels[] = {0.0, 0.04, 0.2, 0.25, 0.49, 0.5, 0.7, 0.75, 0.9, 1};

  for (auto level : levels) {
    // Sets the slider value. `SetValue()` will pass in
    // `SliderChangeReason::kByApi` as the slider change reason and will not
    // trigger UI updates, so we should call `SliderValueChanged()` afterwards
    // to update the icon.
    slider()->SetValue(level);
    brightness_slider_controller()->SliderValueChanged(
        slider(), level, slider()->GetValue(),
        views::SliderChangeReason::kByUser);

    WaitUntilUpdated();

    // The minimum level for brightness is 0.05, since `SliderValueChanged()`
    // will adjust the brightness level and set the icon accordingly.
    const gfx::VectorIcon& icon = GetIcon(std::max(level, kMinBrightnessLevel));

    if (level <= 0.0) {
      EXPECT_STREQ(icon.name,
                   UnifiedBrightnessView::kBrightnessLevelIcons[1]->name);
    } else if (level <= 0.5) {
      EXPECT_STREQ(icon.name,
                   UnifiedBrightnessView::kBrightnessLevelIcons[1]->name);
    } else {
      EXPECT_STREQ(icon.name,
                   UnifiedBrightnessView::kBrightnessLevelIcons[2]->name);
    }
  }
}

// Tests that the `UnifiedBrightnessView` `more_button` is not enabled if and
// only if there is a trusted pinned window.
TEST_F(UnifiedBrightnessViewTest, MoreButton) {
  // At the start of the test, the system tray containing the brightness view is
  // already shown. Since there is no pinned window, the `more_button` should
  // not be disabled.
  EXPECT_TRUE(more_button()->GetEnabled());

  // Close the bubble so the brightness view can be recreated.
  GetPrimaryUnifiedSystemTray()->CloseBubble();

  // Create and trusted pin a window.
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  wm::ActivateWindow(window.get());
  window_util::PinWindow(window.get(), /*trusted=*/true);

  // Open the bubble and check that the new brightness view more button is in
  // the correct state.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_FALSE(more_button()->GetEnabled());

  // Close the bubble so the brightness view can be recreated.
  GetPrimaryUnifiedSystemTray()->CloseBubble();

  // Unpin the window
  WindowState::Get(window.get())->Restore();

  // Make sure the more button is not disabled.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_TRUE(more_button()->GetEnabled());
}

// Tests that the night light button is disabled in the sign-in screen, and is
// enabled in the locked screen.
TEST_F(UnifiedBrightnessViewTest, NightLightButtonState) {
  // Close the bubble so the brightness view can be recreated.
  GetPrimaryUnifiedSystemTray()->CloseBubble();

  // In the sign-in screen, the `night_light_button_` is disabled.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_FALSE(night_light_button()->GetEnabled());

  GetPrimaryUnifiedSystemTray()->CloseBubble();
  // In the locked screen, the `night_light_button_` is enabled.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_TRUE(night_light_button()->GetEnabled());

  GetPrimaryUnifiedSystemTray()->CloseBubble();
  // In the active user session, the `night_light_button_` is enabled.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_TRUE(night_light_button()->GetEnabled());
}

}  // namespace ash
