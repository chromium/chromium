// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/local_authentication_request_controller_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/auth/views/active_session_auth_view.h"
#include "ash/auth/views/auth_container_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/login/ui/local_authentication_request_view.h"
#include "ash/login/ui/local_authentication_request_widget.h"
#include "ash/login/ui/local_authentication_test_api.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login/local_authentication_request_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
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
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

using ::cryptohome::KeyLabel;

const char kTestAccount[] = "user@test.com";
const char kExpectedPassword[] = "qwerty";
const char kExpectedPin[] = "150504";

class LocalAuthenticationRequestControllerImplTest : public LoginTestBase {
 public:
  LocalAuthenticationRequestControllerImplTest(
      const LocalAuthenticationRequestControllerImplTest&) = delete;
  LocalAuthenticationRequestControllerImplTest& operator=(
      const LocalAuthenticationRequestControllerImplTest&) = delete;

 protected:
  LocalAuthenticationRequestControllerImplTest() {
    scoped_features_.InitAndDisableFeature(
        features::kLocalAuthenticationWithPin);
  }

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

  base::test::ScopedFeatureList scoped_features_;
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

// This part is for testing the LocalAuthenticationWithPinController class.

enum class AuthenticationType {
  kPasswordOnly,
  kPinOnly,
  kPasswordAndPin,
};

enum class AuthenticationMethod {
  kPassword,
  kPin,
};

struct AuthenticationParams {
  AuthenticationType type;
  AuthenticationMethod method;
};

std::string AuthenticationParamsToString(
    const testing::TestParamInfo<AuthenticationParams>& info) {
  std::string type_str;
  switch (info.param.type) {
    case AuthenticationType::kPasswordOnly:
      type_str = "PasswordOnly";
      break;
    case AuthenticationType::kPinOnly:
      type_str = "PinOnly";
      break;
    case AuthenticationType::kPasswordAndPin:
      type_str = "PasswordAndPin";
      break;
  }

  std::string method_str;
  switch (info.param.method) {
    case AuthenticationMethod::kPassword:
      method_str = "UsePassword";
      break;
    case AuthenticationMethod::kPin:
      method_str = "UsePin";
      break;
  }

  return type_str + "_" + method_str;
}

std::string GetStubSystemSaltString() {
  return SystemSaltGetter::ConvertRawSaltToHexString(
      FakeCryptohomeMiscClient::GetStubSystemSalt());
}

class AuthSubmissionCounter : public ActiveSessionAuthView::Observer,
                              public views::ViewObserver {
 public:
  explicit AuthSubmissionCounter(ActiveSessionAuthView* view) : view_(view) {
    view_->AddObserver(this);
    contents_view_observer_.Observe(view_);
  }
  ~AuthSubmissionCounter() override {
    if (view_) {
      view_->RemoveObserver(this);
    }
  }

  // ActiveSessionAuthView::Observer:
  void OnPasswordSubmit(const std::u16string& password) override {
    ++password_submit_counter_;
  }

  void OnPinSubmit(const std::u16string& pin) override {
    ++pin_submit_counter_;
  }

  // views::ViewObserver:
  void OnViewRemovedFromWidget(views::View* observed_view) override {
    view_->RemoveObserver(this);
    view_ = nullptr;
    contents_view_observer_.Reset();
  }

  raw_ptr<ActiveSessionAuthView> view_;
  base::ScopedObservation<views::View, ViewObserver> contents_view_observer_{
      this};

  int password_submit_counter_ = 0;
  int pin_submit_counter_ = 0;
};

class LocalAuthenticationWithPinControllerImplTest
    : public NoSessionAshTestBase,
      public LocalAuthenticationWithPinControllerImpl::Observer,
      public ::testing::WithParamInterface<AuthenticationParams> /*auth_params*/
{
 public:
  LocalAuthenticationWithPinControllerImplTest(
      const LocalAuthenticationWithPinControllerImplTest&) = delete;
  LocalAuthenticationWithPinControllerImplTest& operator=(
      const LocalAuthenticationWithPinControllerImplTest&) = delete;

 protected:
  LocalAuthenticationWithPinControllerImplTest() {
    scoped_features_.InitAndEnableFeature(
        features::kLocalAuthenticationWithPin);
  }
  ~LocalAuthenticationWithPinControllerImplTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    CryptohomeMiscClient::InitializeFake();
    FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
    FakeCryptohomeMiscClient::Get()->set_system_salt(
        FakeCryptohomeMiscClient::GetStubSystemSalt());
    UserDataAuthClient::InitializeFake();
    SystemSaltGetter::Initialize();
    test_account_id_ = AccountId::FromUserEmail(kTestAccount);

    SetExpectedCredentialsWithDbusClient();
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager->AddUser(test_account_id_);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    user_manager::KnownUser known_user(Shell::Get()->local_state());
    known_user.SetStringPref(test_account_id_, prefs::kQuickUnlockPinSalt,
                             GetStubSystemSaltString());

    EXPECT_FALSE(IsDialogVisible());

    StartLocalAuthenticationRequest();
    EXPECT_TRUE(IsDialogVisible());

    auto* controller = LocalAuthenticationRequestController::Get();

    if (controller == nullptr) {
      ADD_FAILURE() << "Dialog not exists";
    }
    test_api_ = std::make_unique<LocalAuthenticationWithPinTestApi>(
        static_cast<LocalAuthenticationWithPinControllerImpl*>(controller));
  }

  void TearDown() override {
    EXPECT_FALSE(IsDialogVisible());

    test_api_.reset();
    scoped_user_manager_.reset();
    SystemSaltGetter::Shutdown();
    UserDataAuthClient::Shutdown();
    CryptohomeMiscClient::Shutdown();
    NoSessionAshTestBase::TearDown();
  }

  void SetExpectedCredentialsWithDbusClient() {
    auto* test_api = FakeUserDataAuthClient::TestApi::Get();
    test_api->set_enable_auth_check(true);

    const auto cryptohome_id =
        cryptohome::CreateAccountIdentifierFromAccountId(test_account_id_);

    test_api->AddExistingUser(cryptohome_id);

    if (auth_params_.type != AuthenticationType::kPinOnly) {
      Key key{kExpectedPassword};
      key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                    GetStubSystemSaltString());

      user_data_auth::AuthFactor auth_factor;
      user_data_auth::AuthInput auth_input;

      auth_factor.set_label(ash::kCryptohomeLocalPasswordKeyLabel);
      auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);

      auth_input.mutable_password_input()->set_secret(key.GetSecret());

      // Add the password key to the user.
      test_api->AddAuthFactor(cryptohome_id, auth_factor, auth_input);
    }

    if (auth_params_.type != AuthenticationType::kPasswordOnly) {
      Key key{kExpectedPin};
      key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234,
                    GetStubSystemSaltString());

      user_data_auth::AuthFactor auth_factor;
      user_data_auth::AuthInput auth_input;

      auth_factor.set_label(ash::kCryptohomePinLabel);
      auth_factor.mutable_pin_metadata();
      auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PIN);

      auth_input.mutable_pin_input()->set_secret(key.GetSecret());

      // Add the pin key to the user.
      test_api->AddAuthFactor(cryptohome_id, auth_factor, auth_input);
    }

    session_ids_ = test_api->AddSession(cryptohome_id, false);
  }

  bool IsDialogVisible() const {
    auto* controller = LocalAuthenticationRequestController::Get();
    if (controller == nullptr) {
      ADD_FAILURE() << "Dialog not exists";
      return false;
    }
    return controller->IsDialogVisible();
  }

  void CloseDialog() const {
    EXPECT_TRUE(IsDialogVisible());
    auto* controller = LocalAuthenticationRequestController::Get();
    if (controller == nullptr) {
      ADD_FAILURE() << "Dialog not exists";
    }
    test_api_->Close();
    EXPECT_TRUE(base::test::RunUntil([this]() { return !IsDialogVisible(); }));
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

    std::vector<cryptohome::AuthFactor> factors;

    if (auth_params_.type != AuthenticationType::kPinOnly) {
      // Add local password as an auth factor.
      cryptohome::AuthFactorRef ref(cryptohome::AuthFactorType::kPassword,
                                    KeyLabel(kCryptohomeLocalPasswordKeyLabel));

      cryptohome::AuthFactor factor(ref,
                                    cryptohome::AuthFactorCommonMetadata());
      factors.push_back(factor);

      user_context->SetKey(Key(kExpectedPassword));
      user_context->SetPasswordKey(Key(kExpectedPassword));
    }

    if (auth_params_.type != AuthenticationType::kPasswordOnly) {
      cryptohome::AuthFactorRef ref(cryptohome::AuthFactorType::kPin,
                                    KeyLabel(kCryptohomePinLabel));

      cryptohome::AuthFactor factor(
          ref, cryptohome::AuthFactorCommonMetadata(),
          cryptohome::PinMetadata::Create(
              cryptohome::PinSalt(GetStubSystemSaltString())),
          cryptohome::PinStatus());
      factors.push_back(factor);

      user_context->SetIsUsingPin(true);
    }

    SessionAuthFactors data(factors);
    user_context->SetSessionAuthFactors(data);
    Shell::Get()->local_authentication_request_controller()->ShowWidget(
        base::BindOnce(
            &LocalAuthenticationWithPinControllerImplTest::OnFinished,
            base::Unretained(this)),
        std::move(user_context));
  }

  bool IsPasswordViewVisible() const {
    ActiveSessionAuthView::TestApi contents_view_test_api(
        test_api_->GetContentsView());

    AuthContainerView::TestApi auth_container_view_test_api(
        contents_view_test_api.GetAuthContainerView());

    return auth_container_view_test_api.GetPasswordView()->GetVisible();
  }

  bool IsPinViewVisible() const {
    ActiveSessionAuthView::TestApi contents_view_test_api(
        test_api_->GetContentsView());

    AuthContainerView::TestApi auth_container_view_test_api(
        contents_view_test_api.GetAuthContainerView());

    return auth_container_view_test_api.GetPinContainerView()->GetVisible();
  }

  bool PressSwitchButton() {
    ActiveSessionAuthView::TestApi contents_view_test_api(
        test_api_->GetContentsView());

    AuthContainerView::TestApi auth_container_view_test_api(
        contents_view_test_api.GetAuthContainerView());

    raw_ptr<views::Button> switch_button =
        auth_container_view_test_api.GetSwitchButton();

    if (!switch_button->GetVisible()) {
      return false;
    }

    LeftClickOn(switch_button);
    return true;
  }

  // Simulates entering a password or pin.
  // |success| determines whether the code will be accepted.
  void SimulateValidation(bool success) {
    if (auth_params_.method == AuthenticationMethod::kPassword) {
      EXPECT_TRUE(IsPasswordViewVisible());
      //  Submit password.
      for (char c : kExpectedPassword) {
        ui::KeyboardCode code =
            static_cast<ui::KeyboardCode>(ui::KeyboardCode::VKEY_A + (c - 'a'));
        PressAndReleaseKey(code);
      }
      if (!success) {
        PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
      }
    } else {
      if (auth_params_.type != AuthenticationType::kPinOnly) {
        EXPECT_FALSE(IsPinViewVisible());
        EXPECT_TRUE(PressSwitchButton());
      }
      EXPECT_TRUE(IsPinViewVisible());
      // Submit pin.
      for (char c : kExpectedPin) {
        ui::KeyboardCode code =
            static_cast<ui::KeyboardCode>(ui::KeyboardCode::VKEY_0 + (c - '0'));
        PressAndReleaseKey(code);
      }
      if (!success) {
        PressAndReleaseKey(ui::KeyboardCode::VKEY_3);
      }
    }

    AuthSubmissionCounter auth_submission_counter(test_api_->GetContentsView());

    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);

    EXPECT_TRUE(base::test::RunUntil([&auth_submission_counter]() {
      return (auth_submission_counter.password_submit_counter_ +
              auth_submission_counter.pin_submit_counter_);
    }));
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

  // Access the authentication parameters using auth_params_.
  const AuthenticationParams auth_params_ = GetParam();

  base::test::ScopedFeatureList scoped_features_;

  std::unique_ptr<LocalAuthenticationWithPinTestApi> test_api_;
};

// Tests successful authentication flow.
TEST_P(LocalAuthenticationWithPinControllerImplTest,
       LocalAuthenticationRequestSuccessfulValidation) {
  SimulateValidation(true);
  EXPECT_EQ(1, successful_validation_);
  EXPECT_EQ(0, close_action_);

  // Widget closed after successful validation
  EXPECT_FALSE(IsDialogVisible());
}

// Tests failed authentication flow.
TEST_P(LocalAuthenticationWithPinControllerImplTest,
       LocalAuthenticationRequestFailedValidation) {
  SimulateValidation(false);

  EXPECT_EQ(0, successful_validation_);
  EXPECT_EQ(0, close_action_);

  // Widget still exists despite the auth error.
  EXPECT_TRUE(IsDialogVisible());
  CloseDialog();
}

// Tests close successfully close the widget.
TEST_P(LocalAuthenticationWithPinControllerImplTest,
       LocalAuthenticationRequestClose) {
  CloseDialog();
  EXPECT_TRUE(base::test::RunUntil([this]() { return !IsDialogVisible(); }));

  EXPECT_EQ(0, successful_validation_);
  EXPECT_EQ(1, close_action_);
}

INSTANTIATE_TEST_SUITE_P(
    LocalAuthTest,
    LocalAuthenticationWithPinControllerImplTest,
    testing::Values(AuthenticationParams{AuthenticationType::kPasswordOnly,
                                         AuthenticationMethod::kPassword},
                    AuthenticationParams{AuthenticationType::kPinOnly,
                                         AuthenticationMethod::kPin},
                    AuthenticationParams{AuthenticationType::kPasswordAndPin,
                                         AuthenticationMethod::kPassword},
                    AuthenticationParams{AuthenticationType::kPasswordAndPin,
                                         AuthenticationMethod::kPin}),
    AuthenticationParamsToString);

}  // namespace
}  // namespace ash
