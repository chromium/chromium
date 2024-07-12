// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/auth/views/auth_container_view.h"
#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/pin_keyboard_view.h"
#include "ash/auth/views/test_support/mock_auth_container_view_observer.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/containers/enum_set.h"
#include "base/files/file_path.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class AuthContainerPixelTest : public AshTestBase {
 public:
  AuthContainerPixelTest() = default;
  AuthContainerPixelTest(const AuthContainerPixelTest&) = delete;
  AuthContainerPixelTest& operator=(const AuthContainerPixelTest&) = delete;
  ~AuthContainerPixelTest() override = default;

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->Show();

    container_view_ =
        widget_->SetContentsView(std::make_unique<AuthContainerView>(
            AuthFactorSet{AuthInputType::kPassword, AuthInputType::kPin}));
    test_api_ = std::make_unique<AuthContainerView::TestApi>(container_view_);
    test_api_pin_container_ = std::make_unique<PinContainerView::TestApi>(
        test_api_->GetPinContainerView());
    test_api_pin_keyboard_ = std::make_unique<PinKeyboardView::TestApi>(
        test_api_pin_container_->GetPinKeyboardView());
    test_api_pin_input_ = std::make_unique<AuthInputRowView::TestApi>(
        test_api_pin_container_->GetAuthInputRowView());

    test_api_password_ = std::make_unique<AuthInputRowView::TestApi>(
        test_api_->GetPasswordView());

    // At start the the password is visible and the pin is hidden.
    CHECK(test_api_password_->GetView()->GetVisible());
    CHECK(!test_api_pin_container_->GetView()->GetVisible());
    CHECK(test_api_->GetSwitchButton()->GetVisible());

    // Test the views in day mode.
    DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  }

  void TearDown() override {
    test_api_pin_input_.reset();
    test_api_pin_keyboard_.reset();
    test_api_pin_container_.reset();
    test_api_password_.reset();
    test_api_.reset();
    container_view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<AuthInputRowView::TestApi> test_api_pin_input_;
  std::unique_ptr<PinKeyboardView::TestApi> test_api_pin_keyboard_;
  std::unique_ptr<PinContainerView::TestApi> test_api_pin_container_;
  std::unique_ptr<AuthInputRowView::TestApi> test_api_password_;
  std::unique_ptr<AuthContainerView::TestApi> test_api_;
  raw_ptr<AuthContainerView> container_view_ = nullptr;
};

// Verify the container view with the switch button.
TEST_F(AuthContainerPixelTest, SwitchTest) {
  //  Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "PasswordWithSwitch", /*revision_number=*/0, container_view_));
  // Switch to the pin UI.
  LeftClickOn(test_api_->GetSwitchButton());

  //  Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "PinWithSwitch", /*revision_number=*/0, container_view_));
}

// Verify the PIN only UI.
TEST_F(AuthContainerPixelTest, PinOnlyTest) {
  // Turn off the password factor availability.
  test_api_->GetView()->SetHasPassword(false);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "PinOnly", /*revision_number=*/0, container_view_));
}

// Verify the password only UI.
TEST_F(AuthContainerPixelTest, PasswordOnlyTest) {
  // Turn off the PIN factor availability.
  test_api_->GetView()->SetHasPin(false);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "PasswordOnly", /*revision_number=*/0, container_view_));
}

}  // namespace
}  // namespace ash
