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
#include "ash/strings/grit/ash_strings.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"

namespace ash {

namespace {

using ::cryptohome::KeyLabel;

const char kTestAccount[] = "user@test.com";
const char kExpectedPassword[] = "qwerty";

class LocalAuthenticationRequestControllerImplTest : public LoginTestBase {
 public:
  LocalAuthenticationRequestControllerImplTest(
      const LocalAuthenticationRequestControllerImplTest&) = delete;
  LocalAuthenticationRequestControllerImplTest& operator=(
      const LocalAuthenticationRequestControllerImplTest&) = delete;

 protected:
  LocalAuthenticationRequestControllerImplTest() = default;
  ~LocalAuthenticationRequestControllerImplTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();

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
    LoginTestBase::TearDown();
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
            &LocalAuthenticationRequestControllerImplTest::OnFinished,
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
TEST_F(LocalAuthenticationRequestControllerImplTest,
       LocalAuthenticationRequestDialogFocus) {
  EXPECT_FALSE(LocalAuthenticationRequestWidget::Get());

  StartLocalAuthenticationRequest();

  LocalAuthenticationRequestView* view =
      LocalAuthenticationRequestWidget::GetViewForTesting();
  ASSERT_NE(view, nullptr);

  ASSERT_TRUE(LocalAuthenticationRequestWidget::Get());

  LoginPasswordView* login_password_view =
      LocalAuthenticationRequestView::TestApi(view).login_password_view();
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(login_password_view));

  LocalAuthenticationRequestWidget::Get()->Close(false /* validation success */,
                                                 nullptr);

  EXPECT_FALSE(LocalAuthenticationRequestWidget::Get());
}

// Tests successful authentication flow.
TEST_F(LocalAuthenticationRequestControllerImplTest,
       LocalAuthenticationRequestSuccessfulValidation) {
  EXPECT_FALSE(LocalAuthenticationRequestWidget::Get());

  StartLocalAuthenticationRequest();
  SimulateValidation(true);
  EXPECT_EQ(1, successful_validation_);
  EXPECT_EQ(0, close_action_);

  // Widget closed after successful validation
  EXPECT_FALSE(LocalAuthenticationRequestWidget::Get());
}

// Tests failed authentication flow.
TEST_F(LocalAuthenticationRequestControllerImplTest,
       LocalAuthenticationRequestFailedValidation) {
  EXPECT_FALSE(LocalAuthenticationRequestWidget::Get());

  StartLocalAuthenticationRequest();
  SimulateValidation(false);

  EXPECT_EQ(0, successful_validation_);
  EXPECT_EQ(0, close_action_);

  // Widget still exists despite the auth error.
  EXPECT_TRUE(LocalAuthenticationRequestWidget::Get());

  LocalAuthenticationRequestView* view =
      LocalAuthenticationRequestWidget::GetViewForTesting();
  ASSERT_NE(view, nullptr);
  LocalAuthenticationRequestView::TestApi view_test_api(view);
  EXPECT_EQ(view_test_api.state(), LocalAuthenticationRequestViewState::kError);
}

// Tests close button successfully close the widget.
TEST_F(LocalAuthenticationRequestControllerImplTest,
       LocalAuthenticationRequestClose) {
  EXPECT_FALSE(LocalAuthenticationRequestWidget::Get());

  StartLocalAuthenticationRequest();
  LocalAuthenticationRequestView* view =
      LocalAuthenticationRequestWidget::GetViewForTesting();
  ASSERT_NE(view, nullptr);

  views::Button* close_button =
      LocalAuthenticationRequestView::TestApi(view).close_button();
  SimulateButtonPress(close_button);

  EXPECT_EQ(0, successful_validation_);
  EXPECT_EQ(1, close_action_);

  EXPECT_FALSE(LocalAuthenticationRequestWidget::Get());
}

TEST_F(LocalAuthenticationRequestControllerImplTest,
       LocalAuthenticationRequestViewAccessibleProperties) {
  StartLocalAuthenticationRequest();
  LocalAuthenticationRequestView* view =
      LocalAuthenticationRequestWidget::GetViewForTesting();
  ui::AXNodeData data;

  view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kDialog, data.role);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(
                IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_REQUEST_DESCRIPTION,
                u"user@test.com"));

  data = ui::AXNodeData();
  view->UpdateState(LocalAuthenticationRequestViewState::kNormal, u"",
                    u"Sample Description");
  view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Sample Description");
}

}  // namespace
}  // namespace ash
