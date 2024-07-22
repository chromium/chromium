// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/local_authentication_request_controller_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/login/ui/local_authentication_request_view.h"
#include "ash/login/ui/local_authentication_request_widget.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login/local_authentication_request_controller.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/cryptohome/account_identifier_operators.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/test/button_test_api.h"

namespace ash {

namespace {

using ::cryptohome::KeyLabel;

const char kTestAccount[] = "user@test.com";
const char kExpectedPassword[] = "qwerty";

class LocalAuthenticationRequestControllerImplPixelTest : public AshTestBase {
 public:
  LocalAuthenticationRequestControllerImplPixelTest(
      const LocalAuthenticationRequestControllerImplPixelTest&) = delete;
  LocalAuthenticationRequestControllerImplPixelTest& operator=(
      const LocalAuthenticationRequestControllerImplPixelTest&) = delete;

 protected:
  LocalAuthenticationRequestControllerImplPixelTest() = default;
  ~LocalAuthenticationRequestControllerImplPixelTest() override = default;

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("600x800");
    auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
    dark_light_mode_controller->SetAutoScheduleEnabled(false);
    // Test Base should setup the dark mode.
    EXPECT_EQ(dark_light_mode_controller->IsDarkModeEnabled(), true);

    CryptohomeMiscClient::InitializeFake();
    FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
    FakeCryptohomeMiscClient::Get()->set_system_salt(
        FakeCryptohomeMiscClient::GetStubSystemSalt());
    UserDataAuthClient::InitializeFake();
    SystemSaltGetter::Initialize();

    test_account_id_ = AccountId::FromUserEmail(kTestAccount);

    SetExpectedCredentialsWithDbusClient(test_account_id_, kExpectedPassword);
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager->AddUser(test_account_id_);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  void TearDown() override {
    // If the test did not explicitly dismissed the widget, destroy it now.
    LocalAuthenticationRequestWidget* local_authentication_request_widget =
        LocalAuthenticationRequestWidget::Get();
    if (local_authentication_request_widget) {
      local_authentication_request_widget->Close(false /* validation success */,
                                                 nullptr);
    }
    scoped_user_manager_.reset();
    SystemSaltGetter::Shutdown();
    UserDataAuthClient::Shutdown();
    CryptohomeMiscClient::Shutdown();
    AshTestBase::TearDown();
  }

  void SetExpectedCredentialsWithDbusClient(const AccountId& account_id,
                                            const std::string& password) {
    auto* test_api = FakeUserDataAuthClient::TestApi::Get();
    test_api->set_enable_auth_check(true);

    const auto cryptohome_id =
        cryptohome::CreateAccountIdentifierFromAccountId(account_id);
    Key key{password};
    key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                  SystemSaltGetter::ConvertRawSaltToHexString(
                      FakeCryptohomeMiscClient::GetStubSystemSalt()));

    user_data_auth::AuthFactor auth_factor;
    user_data_auth::AuthInput auth_input;

    auth_factor.set_label(ash::kCryptohomeLocalPasswordKeyLabel);
    auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);

    auth_input.mutable_password_input()->set_secret(key.GetSecret());

    // Add the password key to the user.
    test_api->AddExistingUser(cryptohome_id);
    test_api->AddAuthFactor(cryptohome_id, auth_factor, auth_input);
    session_ids_ = test_api->AddSession(cryptohome_id, false);
  }

  // Simulates mouse press event on a |button|.
  void SimulateButtonPress(views::Button* button) {
    ui::MouseEvent event(/*type=*/ui::EventType::kMousePressed,
                         /*location=*/gfx::Point(),
                         /*root_location=*/gfx::Point(),
                         /*time_stamp=*/ui::EventTimeForNow(),
                         /*flags=*/0,
                         /*changed_button_flags=*/0);
    views::test::ButtonTestApi(button).NotifyClick(event);
  }

  // Called when LocalAuthenticationRequestView finished processing.
  void OnFinished(bool access_granted, std::unique_ptr<UserContext> context) {
    access_granted ? ++successful_validation_ : ++close_action_;
  }

  // Starts local authentication validation.
  void StartLocalAuthenticationRequest() {
    // Configure the user context.
    std::unique_ptr<UserContext> user_context = std::make_unique<UserContext>(
        user_manager::UserType::kRegular, test_account_id_);

    user_context->SetAuthSessionIds(session_ids_.first, session_ids_.second);

    // Add local password as an auth factor.
    std::vector<cryptohome::AuthFactor> factors;
    cryptohome::AuthFactorRef ref(cryptohome::AuthFactorType::kPassword,
                                  KeyLabel(kCryptohomeLocalPasswordKeyLabel));

    cryptohome::AuthFactor factor(ref, cryptohome::AuthFactorCommonMetadata());
    factors.push_back(factor);
    SessionAuthFactors data(factors);
    user_context->SetSessionAuthFactors(data);

    user_context->SetKey(Key(kExpectedPassword));
    user_context->SetPasswordKey(Key(kExpectedPassword));

    Shell::Get()->local_authentication_request_controller()->ShowWidget(
        base::BindOnce(
            &LocalAuthenticationRequestControllerImplPixelTest::OnFinished,
            base::Unretained(this)),
        std::move(user_context));
  }

  // Simulates entering a password. |success| determines whether the code will
  // be accepted.
  void SimulateValidation(bool success) {
    // Submit password.
    for (char c : kExpectedPassword) {
      ui::KeyboardCode code =
          static_cast<ui::KeyboardCode>(ui::KeyboardCode::VKEY_A + (c - 'a'));
      PressAndReleaseKey(code);
    }
    if (!success) {
      PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
    }
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    base::RunLoop().RunUntilIdle();
  }

  // Number of times the view was dismissed with close button.
  int close_action_ = 0;

  // Number of times the view was dismissed after successful validation.
  int successful_validation_ = 0;

  // Test account id.
  AccountId test_account_id_;

  // Auth session ids.
  std::pair<std::string, std::string> session_ids_;

  // Container object for the fake user manager for tests.
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

// Tests local authentication dialog showing/hiding and focus behavior for
// password field
TEST_F(LocalAuthenticationRequestControllerImplPixelTest, FailedValidation) {
  EXPECT_FALSE(LocalAuthenticationRequestWidget::Get());

  StartLocalAuthenticationRequest();

  LocalAuthenticationRequestView* view =
      LocalAuthenticationRequestWidget::GetViewForTesting();
  ASSERT_NE(view, nullptr);
  LocalAuthenticationRequestView::TestApi view_test_api(view);

  // Hide the textfield cursor to avoid the flakiness due to the blinking.
  views::TextfieldTestApi(view_test_api.GetInputTextfield())
      .SetCursorLayerOpacity(0.f);

  ASSERT_TRUE(LocalAuthenticationRequestWidget::Get());

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "Ready", /*revision_number=*/2, view));

  SimulateValidation(false);
  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "Fail", /*revision_number=*/2, view));
}

// Tests local authentication dialog theme change
TEST_F(LocalAuthenticationRequestControllerImplPixelTest, ThemeChange) {
  EXPECT_FALSE(LocalAuthenticationRequestWidget::Get());

  StartLocalAuthenticationRequest();
  LocalAuthenticationRequestView* view =
      LocalAuthenticationRequestWidget::GetViewForTesting();
  ASSERT_NE(view, nullptr);
  LocalAuthenticationRequestView::TestApi view_test_api(view);

  // Hide the textfield cursor to avoid the flakiness due to the blinking.
  views::TextfieldTestApi(view_test_api.GetInputTextfield())
      .SetCursorLayerOpacity(0.f);

  ASSERT_TRUE(LocalAuthenticationRequestWidget::Get());

  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "Light", /*revision_number=*/1, view));
}

}  // namespace
}  // namespace ash
