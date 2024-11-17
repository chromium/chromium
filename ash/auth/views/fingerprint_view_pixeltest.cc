// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/auth/views/fingerprint_view.h"
#include "ash/public/cpp/login_types.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {}  // namespace

class FingerprintViewPixelTest : public AshTestBase {
 public:
  FingerprintViewPixelTest() = default;
  FingerprintViewPixelTest(const FingerprintViewPixelTest&) = delete;
  FingerprintViewPixelTest& operator=(const FingerprintViewPixelTest&) = delete;
  ~FingerprintViewPixelTest() override = default;

 protected:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("600x800");

    widget_ = CreateFramelessTestWidget();
    std::unique_ptr<views::View> container_view =
        std::make_unique<views::View>();

    auto* layout =
        container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);

    container_view->SetPreferredSize(gfx::Size({400, 300}));

    container_view->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemBaseElevated, 0));

    fingerprint_view_ =
        container_view->AddChildView(std::make_unique<FingerprintView>());
    widget_->SetBounds({400, 300});
    widget_->Show();

    container_view_ = widget_->SetContentsView(std::move(container_view));

    auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
    dark_light_mode_controller->SetAutoScheduleEnabled(false);
    // Test Base should setup the dark mode.
    EXPECT_TRUE(dark_light_mode_controller->IsDarkModeEnabled());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    fingerprint_view_ = nullptr;
    container_view_ = nullptr;
    widget_.reset();
  }

  raw_ptr<FingerprintView> fingerprint_view_;
  raw_ptr<views::View> container_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
};

// Verify the AVAILABLE_DEFAULT state.
TEST_F(FingerprintViewPixelTest, AvailableTest) {
  fingerprint_view_->SetState(FingerprintState::AVAILABLE_DEFAULT);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "AvailableDark", /*revision_number=*/0, fingerprint_view_));

  // Switch to day mode.
  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "Available", /*revision_number=*/0, fingerprint_view_));
}

// Verify the AVAILABLE_WITH_TOUCH_SENSOR_WARNING state.
TEST_F(FingerprintViewPixelTest, AvailableWithTouchSensorWarningTest) {
  fingerprint_view_->SetState(FingerprintState::AVAILABLE_DEFAULT);

  fingerprint_view_->SetState(
      FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "AvailableWithTouchSensorWarningDark", /*revision_number=*/0,
      fingerprint_view_));

  // Switch to day mode.
  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "AvailableWithTouchSensorWarning", /*revision_number=*/0,
      fingerprint_view_));
}

// Verify the AVAILABLE_WITH_FAILED_ATTEMPT state/animation.
TEST_F(FingerprintViewPixelTest, AvailableWithFailedAttemptTest) {
  fingerprint_view_->SetState(FingerprintState::AVAILABLE_DEFAULT);

  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  fingerprint_view_->SetState(FingerprintState::AVAILABLE_WITH_FAILED_ATTEMPT);

  FingerprintView::TestApi test_api(fingerprint_view_);
  // To avoid flakiness we just verify the first and last frames.
  test_api.ShowFirstFrame();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "AvailableWithFailedAttemptFirstFrame", /*revision_number=*/1,
      fingerprint_view_));

  test_api.ShowLastFrame();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "AvailableWithFailedAttemptLastFrame", /*revision_number=*/1,
      fingerprint_view_));
}

// Verify the DISABLED_FROM_ATTEMPTS state/animation.
TEST_F(FingerprintViewPixelTest, DisabledFromAttemptsTest) {
  fingerprint_view_->SetState(FingerprintState::AVAILABLE_DEFAULT);

  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  fingerprint_view_->SetState(FingerprintState::DISABLED_FROM_ATTEMPTS);

  // To avoid flakiness we just verify the first and last frames.
  FingerprintView::TestApi test_api(fingerprint_view_);
  test_api.ShowFirstFrame();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "DisabledFromAttemptsFirstFrame", /*revision_number=*/0,
      fingerprint_view_));

  test_api.ShowLastFrame();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "DisabledFromAttemptsLastFrame", /*revision_number=*/0,
      fingerprint_view_));
}

// Verify the DISABLED_FROM_TIMEOUT state.
TEST_F(FingerprintViewPixelTest, DisabledFromTimeoutTest) {
  fingerprint_view_->SetState(FingerprintState::AVAILABLE_DEFAULT);

  fingerprint_view_->SetState(FingerprintState::DISABLED_FROM_TIMEOUT);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "DisabledFromTimeoutDark", /*revision_number=*/0, fingerprint_view_));

  // Switch to day mode.
  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "DisabledFromTimeout", /*revision_number=*/0, fingerprint_view_));
}

}  // namespace ash
