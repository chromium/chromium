// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_contents_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "ash/public/cpp/test/mock_in_session_auth_token_provider.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel.h"
#include "chromeos/ash/components/auth_panel/impl/views/password_auth_view.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/error_types.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/mock_auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "chromeos/ash/components/osauth/public/auth_engine_api.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_hub.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_hub_connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

using ::cryptohome::KeyLabel;
using ::testing::_;

const char kExpectedPassword[] = "expectedpassword";

}  // namespace

class InSessionAuthDialogContentsViewTest : public AshTestBase {
 public:
  InSessionAuthDialogContentsViewTest() = default;

  void SetUp() override {
    AshTestBase::SetUp();
    UserDataAuthClient::InitializeFake();

    auth_hub_ = std::make_unique<MockAuthHub>();
  }

  void TearDown() override {
    // Clean up all UI objects before tearing down Ash Shell in base test.
    auth_panel_test_api_.reset();
    password_auth_view_test_api_.reset();
    contents_view_test_api_.reset();
    contents_view_ = nullptr;
    dialog_.reset();

    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> CreateAuthDialogWidget(
      std::unique_ptr<views::View> contents_view) {
    std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
    widget->SetContentsView(std::move(contents_view));
    return widget;
  }

  void CreateAndShowDialog() {
    auto contents_view = std::make_unique<InSessionAuthDialogContentsView>(
        prompt_,
        base::BindOnce(
            &InSessionAuthDialogContentsViewTest::OnEndAuthentication,
            base::Unretained(this)),
        base::BindRepeating(&InSessionAuthDialogContentsViewTest::
                                OnAuthPanelPreferredSizeChanged,
                            base::Unretained(this)),
        auth_hub_connector_.get(), auth_hub_.get());

    // The order of instantiation is important. Resetting `dialog_`
    // destroys the owned ContentsView, causing our `contents_view_`
    // pointer to dangle.
    contents_view_ = contents_view.get();

    contents_view_test_api_ =
        std::make_unique<InSessionAuthDialogContentsView::TestApi>(
            contents_view_);

    auth_panel_test_api_ =
        std::make_unique<AuthPanel::TestApi>(contents_view_->GetAuthPanel());

    password_auth_view_test_api_.reset();

    dialog_ = CreateAuthDialogWidget(std::move(contents_view));
    CenterDialogOnDisplay();
    dialog_->Show();
  }

  void OnEndAuthentication() { end_authentication_notifications_++; }

  void CenterDialogOnDisplay() {
    auto bounds = display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
    bounds.ClampToCenteredSize(dialog_->GetContentsView()->GetPreferredSize());
    dialog_->SetBounds(bounds);
  }

  void OnAuthPanelPreferredSizeChanged() {
    size_changed_notifications_count_++;
    CenterDialogOnDisplay();
  }

  void InitializeUiWithOnlyPasswordFactor() {
    contents_view_->GetAuthPanel()->InitializeUi(
        AuthFactorsSet{AshAuthFactor::kGaiaPassword},
        auth_hub_connector_.get());
    password_auth_view_test_api_ = std::make_unique<PasswordAuthView::TestApi>(
        auth_panel_test_api_->GetPasswordAuthView());
  }

  void RunPreMouseClickAssertions(views::Button* view) {
    EXPECT_TRUE(view->IsMouseHovered());
    EXPECT_TRUE(view->GetEnabled());
    EXPECT_TRUE(views::InkDrop::Get(view->ink_drop_view())->GetHighlighted());
  }

  void TypePasswordAndAssertEnteredCorrectly(const std::string& password) {
    views::View* password_textfield =
        password_auth_view_test_api_->GetPasswordTextfield();
    LeftClickOn(password_textfield);

    for (char c : password) {
      EXPECT_TRUE(absl::ascii_isalpha(static_cast<unsigned char>(c)));
      GetEventGenerator()->PressAndReleaseKey(
          static_cast<ui::KeyboardCode>(ui::KeyboardCode::VKEY_A + (c - 'a')),
          ui::EF_NONE);
    }

    ASSERT_EQ(password_auth_view_test_api_->GetPasswordTextfield()->GetText(),
              base::UTF8ToUTF16(std::string{kExpectedPassword}));
  }

  void PressCloseButton() {
    auto* generator = GetEventGenerator();
    views::Button* close_dialog_button =
        contents_view_test_api_->GetCloseButton();
    generator->MoveMouseTo(
        close_dialog_button->GetBoundsInScreen().CenterPoint());
    RunPreMouseClickAssertions(close_dialog_button);
    generator->ClickLeftButton();
  }

  void PressAndReleaseEscapeButton() {
    auto* generator = GetEventGenerator();
    generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  }

  void PressAndReleaseEnterButton() {
    auto* generator = GetEventGenerator();
    generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  }

  void SubmitPassword() {
    views::View* submit_password_button =
        password_auth_view_test_api_->GetSubmitPasswordButton();
    LeftClickOn(submit_password_button);
  }

  void NotifyAuthPanelPasswordFactorReady() {
    contents_view_->GetAuthPanel()->OnFactorStatusesChanged(FactorsStatusMap{
        {AshAuthFactor::kGaiaPassword, AuthFactorState::kFactorReady}});
  }

  void NotifyAuthPanelFactorListChanged() {
    // Calling `OnFactorListChanged` will destroy the `PasswordAuthView` to
    // which the test api currently holds a pointer. To avoid dangling,
    // invalidate it and recreate it after rebuilding the UI.
    password_auth_view_test_api_ = nullptr;

    contents_view_->GetAuthPanel()->OnFactorListChanged(FactorsStatusMap{
        {AshAuthFactor::kGaiaPassword, AuthFactorState::kCheckingForPresence},
        {AshAuthFactor::kCryptohomePin,
         AuthFactorState::kCheckingForPresence}});

    password_auth_view_test_api_ = std::make_unique<PasswordAuthView::TestApi>(
        auth_panel_test_api_->GetPasswordAuthView());
  }

  std::unique_ptr<MockAuthHubConnector> auth_hub_connector_;
  std::unique_ptr<MockAuthHub> auth_hub_;

  std::unique_ptr<views::Widget> dialog_;

  raw_ptr<InSessionAuthDialogContentsView> contents_view_;

  std::unique_ptr<InSessionAuthDialogContentsView::TestApi>
      contents_view_test_api_;

  std::unique_ptr<AuthPanel::TestApi> auth_panel_test_api_;

  std::unique_ptr<PasswordAuthView::TestApi> password_auth_view_test_api_;

  int size_changed_notifications_count_ = 0;
  int end_authentication_notifications_ = 0;

 private:
  std::optional<std::string> prompt_;
};

// Tests that the authentication dialog correctly notifies
// AuthHub to canel the current attempt.
TEST_F(InSessionAuthDialogContentsViewTest, CancelDialog) {
  CreateAndShowDialog();
  EXPECT_CALL(*auth_hub_, CancelCurrentAttempt);
  PressCloseButton();
}

// Tests that the authentication dialog correctly notifies
// AuthHub to canel the current attempt twice.
TEST_F(InSessionAuthDialogContentsViewTest, CancelDialogTwice) {
  CreateAndShowDialog();
  EXPECT_CALL(*auth_hub_, CancelCurrentAttempt).Times(1);
  PressCloseButton();

  testing::Mock::VerifyAndClearExpectations(auth_hub_.get());

  // And once again.
  CreateAndShowDialog();
  EXPECT_CALL(*auth_hub_, CancelCurrentAttempt).Times(1);
  PressCloseButton();
}

// Tests that the authentication dialog correctly notifies
// AuthHub to cancel the current attempt when a password is typed into
// the input field.
TEST_F(InSessionAuthDialogContentsViewTest, TypePasswordAndCloseDialog) {
  CreateAndShowDialog();
  InitializeUiWithOnlyPasswordFactor();
  NotifyAuthPanelPasswordFactorReady();
  TypePasswordAndAssertEnteredCorrectly(kExpectedPassword);

  EXPECT_CALL(*auth_hub_, CancelCurrentAttempt);
  ASSERT_EQ(password_auth_view_test_api_->GetPasswordTextfield()->GetText(),
            base::UTF8ToUTF16(std::string{kExpectedPassword}));

  PressCloseButton();
}

// Tests that `AuthPanel` correctly notifies its parent View that
// the preferred size has changed to new factor becoming available, or
// existing factors becoming unavailable.
TEST_F(InSessionAuthDialogContentsViewTest,
       AuthPanelNotifiesToRecenterDialogWhenPrefferredSizeChangesOnInit) {
  CreateAndShowDialog();
  InitializeUiWithOnlyPasswordFactor();
  ASSERT_EQ(size_changed_notifications_count_, 1);
}

// Tests that `AuthPanel` correctly notifies its parent View that
// the preferred size has changed to new factor becoming available, or
// existing factors becoming unavailable.
TEST_F(
    InSessionAuthDialogContentsViewTest,
    AuthPanelNotifiesToRecenterDialogWhenPrefferredSizeChangesOnFactorsChange) {
  CreateAndShowDialog();
  InitializeUiWithOnlyPasswordFactor();
  NotifyAuthPanelFactorListChanged();

  // Called once the UI is initialized with the the initial set of factor, and
  // also when the UI is destroyed and recreated as part of
  // `OnFactorListChanged`.
  ASSERT_EQ(size_changed_notifications_count_, 2);
}

// Tests that `AuthPanel` correctly calls the submit password callback,
// which in production, calls `AuthEngineApi::AuthenticateWithPassword`.
TEST_F(InSessionAuthDialogContentsViewTest, TypePasswordAndSubmit) {
  CreateAndShowDialog();
  InitializeUiWithOnlyPasswordFactor();
  NotifyAuthPanelPasswordFactorReady();
  TypePasswordAndAssertEnteredCorrectly(kExpectedPassword);

  ASSERT_EQ(password_auth_view_test_api_->GetPasswordTextfield()->GetText(),
            base::UTF8ToUTF16(std::string{kExpectedPassword}));

  base::test::TestFuture<AuthHubConnector*, AshAuthFactor, const std::string&>
      future;
  auth_panel_test_api_->SetSubmitPasswordCallback(
      future.GetRepeatingCallback());

  SubmitPassword();

  auto [connector, factor, password] = future.Take();
  EXPECT_EQ(password, std::string{kExpectedPassword});
  EXPECT_EQ(connector, auth_hub_connector_.get());
}

// Tests that `AuthPanel` correctly calls the submit password callback,
// which in production, calls `AuthEngineApi::AuthenticateWithPassword`.
// In this test we show and close the dialog prior to reopening it again and
// submitting a password.
TEST_F(InSessionAuthDialogContentsViewTest, ShowCloseThenSubmitPassword) {
  auto show_and_type_password = [this]() {
    CreateAndShowDialog();
    InitializeUiWithOnlyPasswordFactor();
    NotifyAuthPanelPasswordFactorReady();
    TypePasswordAndAssertEnteredCorrectly(kExpectedPassword);
  };

  show_and_type_password();
  PressCloseButton();

  show_and_type_password();

  base::test::TestFuture<AuthHubConnector*, AshAuthFactor, const std::string&>
      future;
  auth_panel_test_api_->SetSubmitPasswordCallback(
      future.GetRepeatingCallback());

  SubmitPassword();

  auto [connector, factor, password] = future.Take();
  EXPECT_EQ(password, std::string{kExpectedPassword});
  EXPECT_EQ(connector, auth_hub_connector_.get());
}

// Tests that `AuthPanel` correctly notifies parent controller that
// authentication has ended.
TEST_F(InSessionAuthDialogContentsViewTest,
       AuthPanelCorrectlyNotifiesWhenAuthOver) {
  CreateAndShowDialog();
  InitializeUiWithOnlyPasswordFactor();
  NotifyAuthPanelPasswordFactorReady();

  contents_view_->GetAuthPanel()->OnEndAuthentication();

  ASSERT_EQ(end_authentication_notifications_, 1);
}

TEST_F(InSessionAuthDialogContentsViewTest, AuthPanelClosesOnEscapePressed) {
  CreateAndShowDialog();
  InitializeUiWithOnlyPasswordFactor();
  NotifyAuthPanelPasswordFactorReady();

  EXPECT_CALL(*auth_hub_, CancelCurrentAttempt);
  PressAndReleaseEscapeButton();
}

TEST_F(InSessionAuthDialogContentsViewTest,
       AuthPanelSubmitsPasswordOnEnterPressed) {
  CreateAndShowDialog();
  InitializeUiWithOnlyPasswordFactor();
  NotifyAuthPanelPasswordFactorReady();
  TypePasswordAndAssertEnteredCorrectly(kExpectedPassword);

  ASSERT_EQ(password_auth_view_test_api_->GetPasswordTextfield()->GetText(),
            base::UTF8ToUTF16(std::string{kExpectedPassword}));

  base::test::TestFuture<AuthHubConnector*, AshAuthFactor, const std::string&>
      future;
  auth_panel_test_api_->SetSubmitPasswordCallback(
      future.GetRepeatingCallback());

  PressAndReleaseEnterButton();

  auto [connector, factor, password] = future.Take();
  EXPECT_EQ(password, std::string{kExpectedPassword});
  EXPECT_EQ(connector, auth_hub_connector_.get());
}

}  // namespace ash
