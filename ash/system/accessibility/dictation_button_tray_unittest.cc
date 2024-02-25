// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_button_tray.h"
#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/constants/ash_features.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/login_status.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/progress_indicator/progress_indicator.h"
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
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
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

// ProgressIndicatorWaiter -----------------------------------------------------

// A class which supports waiting for a progress indicator to reach a desired
// state of progress.
class ProgressIndicatorWaiter {
 public:
  ProgressIndicatorWaiter() = default;
  ProgressIndicatorWaiter(const ProgressIndicatorWaiter&) = delete;
  ProgressIndicatorWaiter& operator=(const ProgressIndicatorWaiter&) = delete;
  ~ProgressIndicatorWaiter() = default;

  // Waits for `progress_indicator` to reach the specified `progress`. If the
  // `progress_indicator` is already at `progress`, this method no-ops.
  void WaitForProgress(ProgressIndicator* progress_indicator,
                       const std::optional<float>& progress) {
    if (progress_indicator->progress() == progress)
      return;
    base::RunLoop run_loop;
    auto subscription = progress_indicator->AddProgressChangedCallback(
        base::BindLambdaForTesting([&]() {
          if (progress_indicator->progress() == progress)
            run_loop.Quit();
        }));
    run_loop.Run();
  }
};

}  // namespace

// DictationButtonTrayTest -----------------------------------------------------

class DictationButtonTrayTest : public AshTestBase {
 public:
  DictationButtonTrayTest() = default;
  ~DictationButtonTrayTest() override = default;
  DictationButtonTrayTest(const DictationButtonTrayTest&) = delete;
  DictationButtonTrayTest& operator=(const DictationButtonTrayTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    // Focus some input text so the Dictation button will be enabled.
    fake_text_input_client_ =
        std::make_unique<ui::FakeTextInputClient>(ui::TEXT_INPUT_TYPE_TEXT);
    InputMethodAsh ime(nullptr);
    IMEBridge::Get()->SetInputContextHandler(&ime);
    AshTestBase::SetUp();
    FocusTextInputClient();
  }

 protected:
  views::ImageView* GetImageView(DictationButtonTray* tray) {
    return tray->icon_;
  }
  void CheckDictationStatusAndUpdateIcon(DictationButtonTray* tray) {
    tray->CheckDictationStatusAndUpdateIcon();
  }
  void FocusTextInputClient() {
    Shell::Get()
        ->window_tree_host_manager()
        ->input_method()
        ->SetFocusedTextInputClient(fake_text_input_client_.get());
  }
  void DetachTextInputClient() {
    Shell::Get()
        ->window_tree_host_manager()
        ->input_method()
        ->SetFocusedTextInputClient(nullptr);
  }

  std::unique_ptr<ui::FakeTextInputClient> fake_text_input_client_;
};

// Ensures that creation doesn't cause any crashes and adds the image icon.
// Also checks that the tray is visible.
TEST_F(DictationButtonTrayTest, BasicConstruction) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->dictation().SetEnabled(true);
  EXPECT_TRUE(GetImageView(GetTray()));
  EXPECT_TRUE(GetTray()->GetVisible());
}

// Test that clicking the button activates dictation.
TEST_F(DictationButtonTrayTest, ButtonActivatesDictation) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  controller->dictation().SetEnabled(true);
  EXPECT_FALSE(controller->dictation_active());

  GestureTapOn(GetTray());
  EXPECT_TRUE(controller->dictation_active());

  GestureTapOn(GetTray());
  EXPECT_FALSE(controller->dictation_active());
}

// Test that activating dictation causes the button to activate.
TEST_F(DictationButtonTrayTest, ActivatingDictationActivatesButton) {
  AccessibilityController* controller =
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
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  controller->dictation().SetEnabled(true);

  ASSERT_FALSE(controller->dictation_active());
  ASSERT_FALSE(GetTray()->is_active());
  // In an input text area by default.
  EXPECT_TRUE(GetTray()->GetEnabled());

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kEnableOrToggleDictation, {});
  EXPECT_TRUE(controller->dictation_active());
  EXPECT_TRUE(GetTray()->is_active());

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kEnableOrToggleDictation, {});
  EXPECT_FALSE(controller->dictation_active());
  EXPECT_FALSE(GetTray()->is_active());
}

TEST_F(DictationButtonTrayTest, ImageIcons) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  controller->dictation().SetEnabled(true);

  const bool is_jelly_enabled = chromeos::features::IsJellyEnabled();
  const auto* color_provider = GetTray()->GetColorProvider();
  const auto off_icon_color = color_provider->GetColor(
      is_jelly_enabled
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
          : kColorAshIconColorPrimary);
  const auto on_icon_color = color_provider->GetColor(
      is_jelly_enabled ? static_cast<ui::ColorId>(
                             cros_tokens::kCrosSysSystemOnPrimaryContainer)
                       : kColorAshIconColorPrimary);

  gfx::ImageSkia off_icon =
      gfx::CreateVectorIcon(kDictationOffNewuiIcon, off_icon_color);
  gfx::ImageSkia on_icon =
      gfx::CreateVectorIcon(kDictationOnNewuiIcon, on_icon_color);

  views::ImageView* view = GetImageView(GetTray());
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*view->GetImage().bitmap(),
                                         *off_icon.bitmap()));

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kEnableOrToggleDictation, {});

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*view->GetImage().bitmap(),
                                         *on_icon.bitmap()));
}

TEST_F(DictationButtonTrayTest, DisabledWhenNoInputFocused) {
  DetachTextInputClient();

  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->dictation().SetEnabled(true);
  DictationButtonTray* tray = GetTray();
  EXPECT_FALSE(tray->GetEnabled());

  // Action doesn't work because disabled.
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kEnableOrToggleDictation, {});
  EXPECT_FALSE(controller->dictation_active());
  EXPECT_FALSE(tray->GetEnabled());

  FocusTextInputClient();
  EXPECT_TRUE(tray->GetEnabled());

  DetachTextInputClient();
  EXPECT_FALSE(tray->GetEnabled());
}

// Base class for SODA tests of the dictation button tray.
class DictationButtonTraySodaTest : public DictationButtonTrayTest {
 public:
  DictationButtonTraySodaTest() = default;
  ~DictationButtonTraySodaTest() override = default;
  DictationButtonTraySodaTest(const DictationButtonTraySodaTest&) = delete;
  DictationButtonTraySodaTest& operator=(const DictationButtonTraySodaTest&) =
      delete;

  // DictationButtonTrayTest:
  void SetUp() override {
    DictationButtonTrayTest::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        features::kOnDeviceSpeechRecognition);

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

  ProgressIndicator* GetProgressIndicator() {
    return GetTray()->progress_indicator_.get();
  }

  float GetProgressIndicatorProgress() const {
    DCHECK(GetTray()->progress_indicator_);
    std::optional<float> progress = GetTray()->progress_indicator_->progress();
    DCHECK(progress.has_value());
    return progress.value();
  }

  bool IsImageVisible() {
    DCHECK(GetTray()->icon_);

    ui::Layer* const layer = GetTray()->icon_->layer();
    if (!layer)
      return true;

    return layer->GetTargetOpacity() == 1.f &&
           layer->GetTargetTransform() == gfx::Transform();
  }

  bool IsProgressIndicatorVisible() const {
    const float progress = GetProgressIndicatorProgress();
    return progress > 0.f && progress < 1.f;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<speech::SodaInstallerImplChromeOS> soda_installer_impl_;
};

// Tests the behavior of the UpdateOnSpeechRecognitionDownloadChanged() method.
TEST_F(DictationButtonTraySodaTest, UpdateOnSpeechRecognitionDownloadChanged) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->dictation().SetEnabled(true);
  DictationButtonTray* tray = GetTray();
  views::ImageView* image = GetImageView(tray);
  EXPECT_TRUE(IsImageVisible());

  // Download progress of 0 indicates that download is not in-progress.
  tray->UpdateOnSpeechRecognitionDownloadChanged(/*download_progress=*/0);
  EXPECT_EQ(0, tray->download_progress());
  EXPECT_TRUE(tray->GetEnabled());
  EXPECT_EQ(base::UTF8ToUTF16(kEnabledTooltip), image->GetTooltipText());

  // The tray icon should be visible when the download is not in-progress.
  ProgressIndicator* progress_indicator = GetProgressIndicator();
  ProgressIndicatorWaiter().WaitForProgress(
      progress_indicator, ProgressIndicator::kProgressComplete);
  EXPECT_FALSE(IsProgressIndicatorVisible());
  EXPECT_TRUE(IsImageVisible());

  // Any number 0 < number < 100 means that download is in-progress.
  tray->UpdateOnSpeechRecognitionDownloadChanged(/*download_progress=*/50);
  EXPECT_EQ(50, tray->download_progress());
  EXPECT_FALSE(tray->GetEnabled());
  EXPECT_EQ(base::UTF8ToUTF16(kDisabledTooltip), image->GetTooltipText());

  // Enabled state doesn't change even if text input is focused.
  DetachTextInputClient();
  EXPECT_FALSE(tray->GetEnabled());
  FocusTextInputClient();
  EXPECT_FALSE(tray->GetEnabled());

  // The tray icon should still be visible when the download is in progress.
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.5f);
  EXPECT_TRUE(IsProgressIndicatorVisible());
  EXPECT_FALSE(progress_indicator->inner_icon_visible());
  EXPECT_TRUE(IsImageVisible());

  tray->UpdateOnSpeechRecognitionDownloadChanged(/*download_progress=*/70);
  EXPECT_EQ(70, tray->download_progress());
  EXPECT_FALSE(tray->GetEnabled());
  EXPECT_EQ(base::UTF8ToUTF16(kDisabledTooltip), image->GetTooltipText());

  // The tray icon should be visible when the download is in progress.
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.7f);
  EXPECT_TRUE(IsProgressIndicatorVisible());
  EXPECT_FALSE(progress_indicator->inner_icon_visible());
  EXPECT_TRUE(IsImageVisible());

  // Similar to 0, a value of 100 means that download is not in-progress.
  tray->UpdateOnSpeechRecognitionDownloadChanged(/*download_progress=*/100);
  EXPECT_EQ(100, tray->download_progress());
  EXPECT_TRUE(tray->GetEnabled());
  EXPECT_EQ(base::UTF8ToUTF16(kEnabledTooltip), image->GetTooltipText());

  // The tray icon should be visible when the download is not in-progress.
  ProgressIndicatorWaiter().WaitForProgress(
      progress_indicator, ProgressIndicator::kProgressComplete);
  EXPECT_FALSE(IsProgressIndicatorVisible());
  EXPECT_TRUE(IsImageVisible());
}

}  // namespace ash
