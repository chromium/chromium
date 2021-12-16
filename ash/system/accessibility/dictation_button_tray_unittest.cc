// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_button_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/constants/ash_features.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/login_status.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/image_view.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

const std::string kEnabledTooltip = "Dictation";
const std::string kDisabledTooltip = "Downloading speech files";

DictationButtonTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
      ->dictation_button_tray();
}

ui::GestureEvent CreateTapEvent(
    base::TimeDelta delta_from_start = base::TimeDelta()) {
  return ui::GestureEvent(0, 0, 0, base::TimeTicks() + delta_from_start,
                          ui::GestureEventDetails(ui::ET_GESTURE_TAP));
}

}  // namespace

class DictationButtonTrayTest : public AshTestBase {
 public:
  DictationButtonTrayTest() = default;
  ~DictationButtonTrayTest() override = default;
  DictationButtonTrayTest(const DictationButtonTrayTest&) = delete;
  DictationButtonTrayTest& operator=(const DictationButtonTrayTest&) = delete;

  void SetUp() override { AshTestBase::SetUp(); }

 protected:
  views::ImageView* GetImageView(DictationButtonTray* tray) {
    return tray->icon_;
  }
  void CheckDictationStatusAndUpdateIcon(DictationButtonTray* tray) {
    tray->CheckDictationStatusAndUpdateIcon();
  }
};

// Ensures that creation doesn't cause any crashes and adds the image icon.
// Also checks that the tray is visible.
TEST_F(DictationButtonTrayTest, BasicConstruction) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->dictation().SetEnabled(true);
  EXPECT_TRUE(GetImageView(GetTray()));
  EXPECT_TRUE(GetTray()->GetVisible());
}

// Test that clicking the button activates dictation.
TEST_F(DictationButtonTrayTest, ButtonActivatesDictation) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  controller->dictation().SetEnabled(true);
  EXPECT_FALSE(controller->dictation_active());

  GetTray()->PerformAction(CreateTapEvent());
  EXPECT_TRUE(controller->dictation_active());

  GetTray()->PerformAction(CreateTapEvent());
  EXPECT_FALSE(controller->dictation_active());
}

// Test that activating dictation causes the button to activate.
TEST_F(DictationButtonTrayTest, ActivatingDictationActivatesButton) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->dictation().SetEnabled(true);
  Shell::Get()->OnDictationStarted();
  EXPECT_TRUE(GetTray()->is_active());

  Shell::Get()->OnDictationEnded();
  EXPECT_FALSE(GetTray()->is_active());
}

// Tests that the tray only renders as active while dictation is listening. Any
// termination of dictation clears the active state.
TEST_F(DictationButtonTrayTest, ActiveStateOnlyDuringDictation) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  controller->dictation().SetEnabled(true);

  ASSERT_FALSE(controller->dictation_active());
  ASSERT_FALSE(GetTray()->is_active());

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::TOGGLE_DICTATION, {});
  EXPECT_TRUE(controller->dictation_active());
  EXPECT_TRUE(GetTray()->is_active());

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::TOGGLE_DICTATION, {});
  EXPECT_FALSE(controller->dictation_active());
  EXPECT_FALSE(GetTray()->is_active());
}

class DictationButtonTraySodaTest : public DictationButtonTrayTest {
 public:
  DictationButtonTraySodaTest() = default;
  ~DictationButtonTraySodaTest() override = default;
  DictationButtonTraySodaTest(const DictationButtonTraySodaTest&) = delete;
  DictationButtonTraySodaTest& operator=(const DictationButtonTraySodaTest&) =
      delete;

  void SetUp() override {
    DictationButtonTrayTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {::features::kExperimentalAccessibilityDictationOffline,
         features::kOnDeviceSpeechRecognition},
        {});

    // Since this test suite is part of ash unit tests, the
    // SodaInstallerImplChromeOS is never created (it's normally created when
    // `ChromeBrowserMainPartsAsh` initializes). Create it here so that
    // calling speech::SodaInstaller::GetInstance) returns a valid instance.
    soda_installer_impl_ =
        std::make_unique<speech::SodaInstallerImplChromeOS>();
  }

  void TearDown() override {
    soda_installer_impl_.reset();
    AshTestBase::TearDown();
  }

  float GetProgressIndicatorProgress() {
    DCHECK(GetTray()->progress_indicator_);
    absl::optional<float> progress =
        GetTray()->progress_indicator_->CalculateProgress();
    DCHECK(progress.has_value());
    return progress.value();
  }

  bool IsProgressIndicatorVisible() {
    DCHECK(GetTray()->progress_indicator_);
    return GetTray()->progress_indicator_->IsVisible();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<speech::SodaInstallerImplChromeOS> soda_installer_impl_;
};

// Tests the behavior of the UpdateOnSpeechRecognitionDownloadChanged() method.
TEST_F(DictationButtonTraySodaTest, UpdateOnSpeechRecognitionDownloadChanged) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->dictation().SetEnabled(true);
  DictationButtonTray* tray = GetTray();
  views::ImageView* image = GetImageView(tray);

  // Download progress of 0 indicates that download is not in-progress.
  tray->UpdateOnSpeechRecognitionDownloadChanged(/*download_progress=*/0);
  EXPECT_EQ(0, tray->download_progress());
  EXPECT_TRUE(tray->GetEnabled());
  EXPECT_EQ(base::UTF8ToUTF16(kEnabledTooltip), image->GetTooltipText());
  EXPECT_FALSE(IsProgressIndicatorVisible());

  // Any number 0 < number < 100 means that download is in-progress.
  tray->UpdateOnSpeechRecognitionDownloadChanged(/*download_progress=*/50);
  EXPECT_EQ(50, tray->download_progress());
  EXPECT_FALSE(tray->GetEnabled());
  EXPECT_EQ(base::UTF8ToUTF16(kDisabledTooltip), image->GetTooltipText());
  EXPECT_TRUE(IsProgressIndicatorVisible());
  EXPECT_EQ(0.5f, GetProgressIndicatorProgress());

  tray->UpdateOnSpeechRecognitionDownloadChanged(/*download_progress=*/70);
  EXPECT_EQ(70, tray->download_progress());
  EXPECT_FALSE(tray->GetEnabled());
  EXPECT_EQ(base::UTF8ToUTF16(kDisabledTooltip), image->GetTooltipText());
  EXPECT_TRUE(IsProgressIndicatorVisible());
  EXPECT_EQ(0.7f, GetProgressIndicatorProgress());

  // Similar to 0, a value of 100 means that download is not in-progress.
  tray->UpdateOnSpeechRecognitionDownloadChanged(/*download_progress=*/100);
  EXPECT_EQ(100, tray->download_progress());
  EXPECT_TRUE(tray->GetEnabled());
  EXPECT_EQ(base::UTF8ToUTF16(kEnabledTooltip), image->GetTooltipText());
  EXPECT_FALSE(IsProgressIndicatorVisible());
}

}  // namespace ash
