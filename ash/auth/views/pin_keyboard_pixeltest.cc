// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/auth/views/pin_keyboard_view.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/files/file_path.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class PinKeyboardViewObserver : public PinKeyboardView::Observer {
 public:
  void OnDigitButtonPressed(int digit) override {}
  void OnBackspacePressed() override {}
  ~PinKeyboardViewObserver() override = default;
};

class PinKeyboardPixelTest : public AshTestBase {
 public:
  PinKeyboardPixelTest(const PinKeyboardPixelTest&) = delete;
  PinKeyboardPixelTest& operator=(const PinKeyboardPixelTest&) = delete;

 protected:
  PinKeyboardPixelTest() = default;
  ~PinKeyboardPixelTest() override = default;

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("600x800");

    observer_ = std::make_unique<PinKeyboardViewObserver>();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    view_ = widget_->SetContentsView(std::make_unique<PinKeyboardView>());
    view_->AddObserver(observer_.get());

    auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
    dark_light_mode_controller->SetAutoScheduleEnabled(false);
    // Test Base should setup the dark mode.
    EXPECT_EQ(dark_light_mode_controller->IsDarkModeEnabled(), true);
  }

  void TearDown() override {
    view_->RemoveObserver(observer_.get());
    observer_.reset();
    view_ = nullptr;
    widget_.reset();

    AshTestBase::TearDown();
  }

  raw_ptr<PinKeyboardView> view_ = nullptr;
  std::unique_ptr<PinKeyboardViewObserver> observer_;
  std::unique_ptr<views::Widget> widget_;
};

// Verify the pin keyboard component look like in DayMode
TEST_F(PinKeyboardPixelTest, DayMode) {
  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  //  Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "DayMode", /*revision_number=*/3, view_));
}

// Verify the pin keyboard component look like in DayMode
TEST_F(PinKeyboardPixelTest, NightMode) {
  //  Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "NightMode", /*revision_number=*/3, view_));
}

}  // namespace
}  // namespace ash
