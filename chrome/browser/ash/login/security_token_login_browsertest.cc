// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/saml/security_token_saml_test.h"
#include "chrome/browser/ash/login/security_token_session_controller.h"
#include "chrome/browser/ash/login/security_token_session_controller_factory.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/certificate_provider/test_certificate_provider_extension_mixin.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/challenge_response/key_label_utils.h"
#include "chromeos/ash/components/login/auth/challenge_response/known_user_pref_utils.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/features/simple_feature.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace ash {

namespace {

// The PIN code that the test certificate provider extension is configured to
// expect.
constexpr char kCorrectPin[] = "17093";
constexpr char kWrongPin[] = "1234";

// UI golden strings in the en-US locale:
constexpr char16_t kChallengeResponseLoginLabel[] = u"Sign in with smart card";
constexpr char16_t kChallengeResponseErrorLabel[] =
    u"Couldn’t recognize your smart card. Try again.";
constexpr char16_t kPinDialogDefaultTitle[] = u"Smart card PIN";
constexpr char16_t kPinDialogInvalidPinTitle[] = u"Invalid PIN.";
constexpr char16_t kPinDialogInvalidPin2AttemptsTitle[] =
    u"Invalid PIN. 2 attempts left";

constexpr char16_t kPinDialogInvalidPin1AttemptTitle[] =
    u"Invalid PIN. 1 attempt left";
constexpr char16_t kPinDialogNoAttemptsLeftTitle[] =
    u"Maximum allowed attempts exceeded.";

constexpr char kChallengeData[] = "challenge";

// The time a test waits to verify that the session was *not* terminated.
// This timeout doesn't lead to flakiness by itself because the longer the
// timeout is, the higher the probability is a regression is caught by the
// test.
constexpr base::TimeDelta kTimeUntilIdle = base::Milliseconds(100);

// Returns the profile into which login-screen extensions are force-installed.
Profile* GetOriginalSigninProfile() {
  return ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

// Custom implementation of the UserDataAuthClient that triggers the
// challenge-response protocol when authenticating the user.
class ChallengeResponseFakeUserDataAuthClient : public FakeUserDataAuthClient {
 public:
  ChallengeResponseFakeUserDataAuthClient() = default;
  ChallengeResponseFakeUserDataAuthClient(
      const ChallengeResponseFakeUserDataAuthClient&) = delete;
  ChallengeResponseFakeUserDataAuthClient& operator=(
      const ChallengeResponseFakeUserDataAuthClient&) = delete;
  ~ChallengeResponseFakeUserDataAuthClient() override = default;

  void set_challenge_response_account_id(const AccountId& account_id) {
    challenge_response_account_id_ = account_id;
  }

  void AuthenticateAuthFactor(
      const ::user_data_auth::AuthenticateAuthFactorRequest& request,
      AuthenticateAuthFactorCallback callback) override {
    chromeos::CertificateProviderService* certificate_provider_service =
        chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
            GetOriginalSigninProfile());
    // Note: The real cryptohome would call the "ChallengeKey" D-Bus method
    // exposed by Chrome via org.chromium.CryptohomeKeyDelegateInterface, but
    // we're directly requesting the extension in order to avoid extra
    // complexity in this UI-oriented browser test.
    certificate_provider_service->RequestSignatureBySpki(
        TestCertificateProviderExtension::GetCertificateSpki(),
        SSL_SIGN_RSA_PKCS1_SHA256,
        base::as_bytes(base::make_span(kChallengeData)),
        challenge_response_account_id_,
        base::BindOnce(&ChallengeResponseFakeUserDataAuthClient::
                           ContinueAuthenticateFactorWithSignature,
                       base::Unretained(this), request, std::move(callback)));
  }

  void AddAuthFactor(const ::user_data_auth::AddAuthFactorRequest& request,
                     AddAuthFactorCallback callback) override {
    FAIL() << "Should not be called";
  }

 private:
  void ContinueAuthenticateFactorWithSignature(
      const ::user_data_auth::AuthenticateAuthFactorRequest& request,
      AuthenticateAuthFactorCallback callback,
      net::Error error,
      const std::vector<uint8_t>& signature) {
    if (error != net::OK || signature.empty()) {
      ::user_data_auth::AuthenticateAuthFactorReply reply;
      reply.set_error(
          ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_MOUNT_FATAL);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), reply));
      return;
    }
    FakeUserDataAuthClient::AuthenticateAuthFactor(request,
                                                   std::move(callback));
  }
  AccountId challenge_response_account_id_;
};

// Helper that allows to wait until the authentication failure is reported.
class AuthFailureWaiter final : public AuthStatusConsumer {
 public:
  AuthFailureWaiter() {
    ExistingUserController::current_controller()->AddLoginStatusConsumer(this);
  }

  AuthFailureWaiter(const AuthFailureWaiter&) = delete;
  AuthFailureWaiter& operator=(const AuthFailureWaiter&) = delete;

  ~AuthFailureWaiter() override {
    ExistingUserController::current_controller()->RemoveLoginStatusConsumer(
        this);
  }

  AuthFailure::FailureReason Wait() {
    run_loop_.Run();
    return failure_reason_;
  }

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override {
    failure_reason_ = error.reason();
    run_loop_.Quit();
  }
  void OnAuthSuccess(const UserContext& user_context) override {}

 private:
  base::RunLoop run_loop_;
  AuthFailure::FailureReason failure_reason_ = AuthFailure::NONE;
};

// A helper class that blocks execution until Chrome is locking or terminating.
class ChromeSessionObserver : public SessionObserver {
 public:
  ChromeSessionObserver() { SessionController::Get()->AddObserver(this); }

  ChromeSessionObserver(const ChromeSessionObserver&) = delete;
  ChromeSessionObserver& operator=(const ChromeSessionObserver&) = delete;

  ~ChromeSessionObserver() override {
    SessionController::Get()->RemoveObserver(this);
  }

  void WaitForSessionLocked() { session_locked_loop_.Run(); }

  void WaitForChromeTerminating() { termination_loop_.Run(); }

  // There is no good way of checking Chrome won't terminate after some timeout.
  // Therefore, this waits an approximate time until the worker thread should
  // become idle in the current test setup.
  void WaitAndAssertChromeNotTerminating() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kTimeUntilIdle);
    run_loop.Run();

    ASSERT_FALSE(is_chrome_terminating());
  }

  bool is_chrome_terminating() const { return is_chrome_terminating_; }

  // SessionObserver
  void OnChromeTerminating() override {
    is_chrome_terminating_ = true;
    termination_loop_.Quit();
  }

  void OnSessionStateChanged(session_manager::SessionState state) override {
    if (state == session_manager::SessionState::LOCKED)
      session_locked_loop_.Quit();
  }

 private:
  bool is_chrome_terminating_ = false;
  base::RunLoop session_locked_loop_;
  base::RunLoop termination_loop_;
};

}  // namespace

// Tests the challenge-response based login (e.g., using a smart card) for an
// existing user.
class SecurityTokenLoginTest : public MixinBasedInProcessBrowserTest,
                               public LocalStateMixin::Delegate {
 protected:
  SecurityTokenLoginTest() {
    auto cryptohome_client =
        std::make_unique<ChallengeResponseFakeUserDataAuthClient>();
    cryptohome_client_ = cryptohome_client.get();
    FakeUserDataAuthClient::TestApi::OverrideGlobalInstance(
        std::move(cryptohome_client));

    // Don't shut down when no browser is open, since it breaks the test and
    // since it's not the real Chrome OS behavior.
    set_exit_when_last_browser_closes(false);

    login_manager_mixin_.AppendManagedUsers(1);
    cryptohome_client_->set_challenge_response_account_id(
        GetChallengeResponseAccountId());
    RegisterCryptohomeKey();
  }

  SecurityTokenLoginTest(const SecurityTokenLoginTest&) = delete;
  SecurityTokenLoginTest& operator=(const SecurityTokenLoginTest&) = delete;
  ~SecurityTokenLoginTest() override = default;

  // MixinBasedInProcessBrowserTest:

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);

    // Avoid aborting the user sign-in due to the user policy requests not being
    // faked in the test.
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    // Init the user policy provider.
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    WaitForLoginScreenWidgetShown();
  }

  // LocalStateMixin::Delegate:

  void SetUpLocalState() override { RegisterChallengeResponseKey(); }

  AccountId GetChallengeResponseAccountId() const {
    return login_manager_mixin_.users()[0].account_id;
  }

  TestCertificateProviderExtension* certificate_provider_extension() {
    return test_certificate_provider_extension_mixin_.extension();
  }

  void RegisterCryptohomeKey() {
    user_data_auth::AuthFactor auth_factor;
    user_data_auth::AuthInput auth_input;
    ChallengeResponseKey challenge_response_key;
    challenge_response_key.set_public_key_spki_der(
        certificate_provider_extension()->GetCertificateSpki());

    auth_factor.set_label(
        GenerateChallengeResponseKeyLabel({challenge_response_key}));
    auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_SMART_CARD);
    auth_factor.mutable_smart_card_metadata()->set_public_key_spki_der(
        TestCertificateProviderExtension::GetCertificateSpki());

    FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(
        cryptohome::CreateAccountIdentifierFromAccountId(
            GetChallengeResponseAccountId()),
        auth_factor, auth_input);
  }

  void StartLoginAndWaitForPinDialog() {
    base::RunLoop pin_dialog_waiting_run_loop;
    LoginScreenTestApi::SetPinRequestWidgetShownCallback(
        pin_dialog_waiting_run_loop.QuitClosure());
    LoginScreenTestApi::ClickChallengeResponseButton(
        GetChallengeResponseAccountId());
    pin_dialog_waiting_run_loop.Run();
  }

  void WaitForChallengeResponseLabel(const std::u16string& awaited_label) {
    test::TestPredicateWaiter waiter(base::BindRepeating(
        [](const AccountId& account_id, const std::u16string& awaited_label) {
          return LoginScreenTestApi::GetChallengeResponseLabel(account_id) ==
                 awaited_label;
        },
        GetChallengeResponseAccountId(), awaited_label));
    waiter.Wait();
  }

  void WaitForPinDialogTitle(const std::u16string& awaited_title) {
    test::TestPredicateWaiter waiter(base::BindRepeating(
        [](const std::u16string& awaited_title) {
          return LoginScreenTestApi::GetPinRequestWidgetTitle() ==
                 awaited_title;
        },
        awaited_title));
    waiter.Wait();
  }

  void WaitForActiveSession() { login_manager_mixin_.WaitForActiveSession(); }

  // Configures and installs the login screen certificate provider extension.
  void PrepareCertificateProviderExtension() {
    extension_force_install_mixin_.InitWithMockPolicyProvider(
        GetOriginalSigninProfile(), policy_provider());
    ASSERT_NO_FATAL_FAILURE(
        test_certificate_provider_extension_mixin_.ForceInstall(
            GetOriginalSigninProfile()));
    certificate_provider_extension()->set_require_pin(kCorrectPin);
  }

  // Waits until the Login or Lock screen is shown.
  void WaitForLoginScreenWidgetShown() {
    base::RunLoop run_loop;
    LoginScreenTestApi::AddOnLockScreenShownCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  LoginManagerMixin* login_manager_mixin() { return &login_manager_mixin_; }

  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &policy_provider_;
  }

 private:
  void RegisterChallengeResponseKey() {
    ChallengeResponseKey challenge_response_key;
    challenge_response_key.set_public_key_spki_der(
        TestCertificateProviderExtension::GetCertificateSpki());
    challenge_response_key.set_extension_id(
        TestCertificateProviderExtension::extension_id());

    base::Value::List challenge_response_keys_list =
        SerializeChallengeResponseKeysForKnownUser({challenge_response_key});
    user_manager::KnownUser(g_browser_process->local_state())
        .SetChallengeResponseKeys(GetChallengeResponseAccountId(),
                                  std::move(challenge_response_keys_list));
  }

  // Bypass "signin_screen" feature only enabled for allowlisted extensions.
  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest
      feature_allowlist_{TestCertificateProviderExtension::extension_id()};

  // Unowned (referencing a global singleton)
  raw_ptr<ChallengeResponseFakeUserDataAuthClient, DanglingUntriaged>
      cryptohome_client_ = nullptr;
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{&mixin_host_,
                                         {},
                                         nullptr,
                                         &cryptohome_mixin_};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  TestCertificateProviderExtensionMixin
      test_certificate_provider_extension_mixin_{
          &mixin_host_, &extension_force_install_mixin_};
};

// Tests the successful challenge-response login flow, including entering the
// correct PIN.
IN_PROC_BROWSER_TEST_F(SecurityTokenLoginTest, Basic) {
  PrepareCertificateProviderExtension();

  // The user pod is displayed with the challenge-response "start" button
  // instead of the password input field.
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(GetChallengeResponseAccountId()));
  EXPECT_FALSE(LoginScreenTestApi::IsPasswordFieldShown(
      GetChallengeResponseAccountId()));
  EXPECT_EQ(LoginScreenTestApi::GetChallengeResponseLabel(
                GetChallengeResponseAccountId()),
            kChallengeResponseLoginLabel);

  // The challenge-response "start" button is clicked. The MountEx request is
  // sent to cryptohome, and in turn cryptohome makes a challenge request. The
  // certificate provider extension receives this request and requests the PIN
  // dialog.
  StartLoginAndWaitForPinDialog();
  EXPECT_EQ(LoginScreenTestApi::GetPinRequestWidgetTitle(),
            kPinDialogDefaultTitle);

  // The PIN is entered.
  LoginScreenTestApi::SubmitPinRequestWidget(kCorrectPin);

  // The PIN is received by the certificate provider extension, which replies to
  // the challenge request. cryptohome receives this response and completes the
  // MountEx request. The user session begins.
  WaitForActiveSession();
}

// Test the login failure scenario when the certificate provider extension is
// missing.
IN_PROC_BROWSER_TEST_F(SecurityTokenLoginTest, MissingExtension) {
  EXPECT_EQ(LoginScreenTestApi::GetChallengeResponseLabel(
                GetChallengeResponseAccountId()),
            kChallengeResponseLoginLabel);

  LoginScreenTestApi::ClickChallengeResponseButton(
      GetChallengeResponseAccountId());
  // An error will be shown after the login attempt gets rejected (note that the
  // rejection happens before the actual authentication begins, which is why
  // AuthFailureWaiter cannot be used in this test).
  WaitForChallengeResponseLabel(kChallengeResponseErrorLabel);
}

// Test the login failure scenario when the PIN dialog gets canceled.
IN_PROC_BROWSER_TEST_F(SecurityTokenLoginTest, PinCancel) {
  PrepareCertificateProviderExtension();
  StartLoginAndWaitForPinDialog();

  // The PIN dialog is canceled. The login attempt is aborted.
  AuthFailureWaiter auth_failure_waiter;
  LoginScreenTestApi::CancelPinRequestWidget();
  EXPECT_EQ(auth_failure_waiter.Wait(),
            AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);

  // No error is shown, and a new login attempt is allowed.
  EXPECT_TRUE(LoginScreenTestApi::IsChallengeResponseButtonClickable(
      GetChallengeResponseAccountId()));
  EXPECT_EQ(LoginScreenTestApi::GetChallengeResponseLabel(
                GetChallengeResponseAccountId()),
            kChallengeResponseLoginLabel);
}

// Test the successful login scenario when the correct PIN was entered only on
// the second attempt.
IN_PROC_BROWSER_TEST_F(SecurityTokenLoginTest, WrongPinThenCorrect) {
  PrepareCertificateProviderExtension();
  StartLoginAndWaitForPinDialog();

  // A wrong PIN is entered, and an error is shown in the PIN dialog.
  LoginScreenTestApi::SubmitPinRequestWidget(kWrongPin);
  WaitForPinDialogTitle(kPinDialogInvalidPinTitle);

  // The correct PIN is entered, and the login succeeds.
  LoginScreenTestApi::SubmitPinRequestWidget(kCorrectPin);
  WaitForActiveSession();
}

// Test the login failure scenario when the wrong PIN is entered several times
// until there's no more attempt left (simulating, e.g., a smart card lockout).
IN_PROC_BROWSER_TEST_F(SecurityTokenLoginTest, WrongPinUntilLockout) {
  PrepareCertificateProviderExtension();
  certificate_provider_extension()->set_remaining_pin_attempts(3);

  StartLoginAndWaitForPinDialog();

  // A wrong PIN is entered several times, causing a corresponding error
  // displayed in the PIN dialog.
  LoginScreenTestApi::SubmitPinRequestWidget(kWrongPin);
  WaitForPinDialogTitle(kPinDialogInvalidPin2AttemptsTitle);
  LoginScreenTestApi::SubmitPinRequestWidget(kWrongPin);
  WaitForPinDialogTitle(kPinDialogInvalidPin1AttemptTitle);
  LoginScreenTestApi::SubmitPinRequestWidget(kWrongPin);
  WaitForPinDialogTitle(kPinDialogNoAttemptsLeftTitle);

  // After closing the PIN dialog with the fatal error, the login fails.
  AuthFailureWaiter auth_failure_waiter;
  LoginScreenTestApi::CancelPinRequestWidget();
  EXPECT_EQ(auth_failure_waiter.Wait(),
            AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
}

// Test the login failure scenario when the extension fails to sign the
// challenge.
IN_PROC_BROWSER_TEST_F(SecurityTokenLoginTest, SigningFailure) {
  PrepareCertificateProviderExtension();
  certificate_provider_extension()->set_should_fail_sign_digest_requests(true);

  AuthFailureWaiter auth_failure_waiter;
  LoginScreenTestApi::ClickChallengeResponseButton(
      GetChallengeResponseAccountId());
  EXPECT_EQ(auth_failure_waiter.Wait(),
            AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);

  // An error is shown.
  EXPECT_TRUE(LoginScreenTestApi::IsChallengeResponseButtonClickable(
      GetChallengeResponseAccountId()));
  EXPECT_EQ(LoginScreenTestApi::GetChallengeResponseLabel(
                GetChallengeResponseAccountId()),
            kChallengeResponseErrorLabel);
}

// Tests for the SecurityTokenSessionBehavior and
// SecurityTokenSessionNotificationSeconds policies.
class SecurityTokenSessionBehaviorTest : public SecurityTokenLoginTest {
 protected:
  SecurityTokenSessionBehaviorTest() = default;
  SecurityTokenSessionBehaviorTest(const SecurityTokenSessionBehaviorTest&) =
      delete;
  SecurityTokenSessionBehaviorTest& operator=(
      const SecurityTokenSessionBehaviorTest&) = delete;
  ~SecurityTokenSessionBehaviorTest() override = default;

  void Login() {
    PrepareCertificateProviderExtension();
    StartLoginAndWaitForPinDialog();
    LoginScreenTestApi::SubmitPinRequestWidget(kCorrectPin);
    WaitForActiveSession();
    profile_ = ProfileHelper::Get()->GetProfileByAccountId(
        GetChallengeResponseAccountId());
  }

  void Lock() {
    ScreenLockerTester().Lock();
    // Mimic the behavior of real-world smart card middleware extensions, which
    // stop talking to smart cards in-session while at the lock screen.
    SetSecurityTokenAvailability(/*available_on_login_screen=*/true,
                                 /*available_in_session=*/false);
  }

  // Configures and installs the user session certificate provider extension.
  void PrepareUserCertificateProviderExtension(
      bool immediately_provide_certificates = true) {
    user_extension_mixin_.InitWithMockPolicyProvider(profile(),
                                                     policy_provider());
    ASSERT_NO_FATAL_FAILURE(
        test_certificate_provider_extension_mixin_.ForceInstall(
            profile(), /*wait_on_extension_loaded=*/true,
            immediately_provide_certificates));
  }

  // Makes the extensions call certificateProvider.setCertificates(). Depending
  // on the passed flags, extensions will pass empty or non-empty sets of
  // certificates.
  void SetSecurityTokenAvailability(bool available_on_login_screen,
                                    bool available_in_session) {
    // Configure the sign-in screen extension.
    ASSERT_TRUE(certificate_provider_extension());
    certificate_provider_extension()->set_should_provide_certificates(
        available_on_login_screen);
    certificate_provider_extension()->TriggerSetCertificates();
    // Configure the in-session extension.
    ASSERT_TRUE(user_certificate_provider_extension());
    user_certificate_provider_extension()->set_should_provide_certificates(
        available_in_session);
    user_certificate_provider_extension()->TriggerSetCertificates();
  }

  bool ProfileHasNotification(Profile* profile,
                              const std::string& notification_id) {
    NotificationDisplayService* notification_display_service =
        NotificationDisplayServiceFactory::GetForProfile(profile);
    if (!notification_display_service) {
      ADD_FAILURE() << "NotificationDisplayService could not be found.";
      return false;
    }
    base::RunLoop run_loop;
    bool has_notification = false;
    notification_display_service->GetDisplayed(
        base::BindLambdaForTesting([&](std::set<std::string> notification_ids,
                                       bool /* supports_synchronization */) {
          has_notification = notification_ids.count(notification_id) >= 1;
          run_loop.Quit();
        }));

    run_loop.Run();
    return has_notification;
  }

  bool GetNotificationDisplayedKnownUserFlag() const {
    return user_manager::KnownUser(g_browser_process->local_state())
        .FindBoolPath(GetChallengeResponseAccountId(),
                      login::SecurityTokenSessionController::
                          kNotificationDisplayedKnownUserKey)
        .value_or(false);
  }

  Profile* profile() const { return profile_; }

  TestCertificateProviderExtension* user_certificate_provider_extension() {
    return test_certificate_provider_extension_mixin_.extension();
  }

 private:
  ExtensionForceInstallMixin user_extension_mixin_{&mixin_host_};
  TestCertificateProviderExtensionMixin
      test_certificate_provider_extension_mixin_{&mixin_host_,
                                                 &user_extension_mixin_};

  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
};

// Tests the SecurityTokenSessionBehavior policy with value "LOCK".
IN_PROC_BROWSER_TEST_F(SecurityTokenSessionBehaviorTest, Lock) {
  Login();
  g_browser_process->local_state()->SetString(
      prefs::kSecurityTokenSessionBehavior, "LOCK");
  PrepareUserCertificateProviderExtension();
  SetSecurityTokenAvailability(/*available_on_login_screen=*/false,
                               /*available_in_session=*/true);
  ChromeSessionObserver chrome_session_observer;

  SetSecurityTokenAvailability(/*available_on_login_screen=*/false,
                               /*available_in_session=*/false);
  chrome_session_observer.WaitForSessionLocked();

  EXPECT_TRUE(ProfileHasNotification(
      profile(), "security_token_session_controller_notification"));
  EXPECT_TRUE(GetNotificationDisplayedKnownUserFlag());
}

// Tests the SecurityTokenSessionBehavior policy with value "LOGOUT".
IN_PROC_BROWSER_TEST_F(SecurityTokenSessionBehaviorTest, PRE_Logout) {
  Login();
  ChromeSessionObserver chrome_session_observer;
  g_browser_process->local_state()->SetString(
      prefs::kSecurityTokenSessionBehavior, "LOGOUT");
  PrepareUserCertificateProviderExtension(
      /*immediately_provide_certificates=*/false);
  SetSecurityTokenAvailability(/*available_on_login_screen=*/false,
                               /*available_in_session=*/true);
  chrome_session_observer.WaitAndAssertChromeNotTerminating();

  login::SecurityTokenSessionControllerFactory::GetForBrowserContext(profile())
      ->TriggerSessionActivationTimeoutForTest();
  chrome_session_observer.WaitAndAssertChromeNotTerminating();

  // Removal of the certificate should lead to the end of the current session.
  SetSecurityTokenAvailability(/*available_on_login_screen=*/false,
                               /*available_in_session=*/false);
  chrome_session_observer.WaitForChromeTerminating();

  // Check login screen notification is scheduled.
  EXPECT_TRUE(GetNotificationDisplayedKnownUserFlag());
}

IN_PROC_BROWSER_TEST_F(SecurityTokenSessionBehaviorTest, Logout) {
  // Check login screen notification is displayed.
  EXPECT_TRUE(
      ProfileHasNotification(GetOriginalSigninProfile(),
                             "security_token_session_controller_notification"));
}

// Test that entering a session with a missing certificate logs user out only
// after an activation timeout and not sooner. This tests the scenario from
// b/331661783.
IN_PROC_BROWSER_TEST_F(SecurityTokenSessionBehaviorTest,
                       LogoutAfterSessionActivation) {
  Login();
  ChromeSessionObserver chrome_session_observer;
  g_browser_process->local_state()->SetString(
      prefs::kSecurityTokenSessionBehavior, "LOGOUT");
  PrepareUserCertificateProviderExtension(
      /*immediately_provide_certificates=*/false);
  SetSecurityTokenAvailability(/*available_on_login_screen=*/false,
                               /*available_in_session=*/false);
  chrome_session_observer.WaitAndAssertChromeNotTerminating();

  login::SecurityTokenSessionControllerFactory::GetForBrowserContext(profile())
      ->TriggerSessionActivationTimeoutForTest();
  chrome_session_observer.WaitForChromeTerminating();
}

// Test that entering a session with a missing certificate locks screen after
// session activation timer.
IN_PROC_BROWSER_TEST_F(SecurityTokenSessionBehaviorTest,
                       LockAfterSessionActivation) {
  Login();
  ChromeSessionObserver chrome_session_observer;
  g_browser_process->local_state()->SetString(
      prefs::kSecurityTokenSessionBehavior, "LOCK");
  login::SecurityTokenSessionControllerFactory::GetForBrowserContext(profile())
      ->SetSessionActivationTimeoutForTest(base::Seconds(0));

  PrepareUserCertificateProviderExtension(
      /*immediately_provide_certificates=*/false);
  SetSecurityTokenAvailability(/*available_on_login_screen=*/false,
                               /*available_in_session=*/false);
  chrome_session_observer.WaitForSessionLocked();
}

// Test that entering the Lock Screen doesn't cause the logout if the policy is
// set to LOGOUT. This is a regression test for crbug.com/1349140.
IN_PROC_BROWSER_TEST_F(SecurityTokenSessionBehaviorTest,
                       LockScreenWhileLogoutPolicy) {
  Login();
  g_browser_process->local_state()->SetString(
      prefs::kSecurityTokenSessionBehavior, "LOGOUT");
  PrepareUserCertificateProviderExtension();
  SetSecurityTokenAvailability(/*available_on_login_screen=*/false,
                               /*available_in_session=*/true);
  ChromeSessionObserver chrome_session_observer;
  Lock();

  chrome_session_observer.WaitAndAssertChromeNotTerminating();
  EXPECT_FALSE(GetNotificationDisplayedKnownUserFlag());
}

// Test that removing the security token while at the Lock Screen causes the
// logout if the policy is set to LOGOUT.
IN_PROC_BROWSER_TEST_F(SecurityTokenSessionBehaviorTest, LogoutFromLockScreen) {
  Login();
  g_browser_process->local_state()->SetString(
      prefs::kSecurityTokenSessionBehavior, "LOGOUT");
  PrepareUserCertificateProviderExtension();
  SetSecurityTokenAvailability(/*available_on_login_screen=*/false,
                               /*available_in_session=*/true);
  ChromeSessionObserver chrome_session_observer;
  Lock();

  // Removal of the certificate should lead to the end of the current session.
  SetSecurityTokenAvailability(/*available_on_login_screen=*/false,
                               /*available_in_session=*/false);
  chrome_session_observer.WaitForChromeTerminating();

  // Check login screen notification is scheduled.
  EXPECT_TRUE(GetNotificationDisplayedKnownUserFlag());
}

// Tests the SecurityTokenSessionNotificationSeconds policy.
IN_PROC_BROWSER_TEST_F(SecurityTokenSessionBehaviorTest, NotificationSeconds) {
  Login();
  g_browser_process->local_state()->SetString(
      prefs::kSecurityTokenSessionBehavior, "LOCK");
  g_browser_process->local_state()->SetInteger(
      prefs::kSecurityTokenSessionNotificationSeconds, 1);
  PrepareUserCertificateProviderExtension();
  ChromeSessionObserver chrome_session_observer;

  views::NamedWidgetShownWaiter notification_waiter(
      views::test::AnyWidgetTestPasskey{},
      "SecurityTokenSessionRestrictionView");

  SetSecurityTokenAvailability(/*available_on_login_screen=*/false,
                               /*available_in_session=*/false);

  views::Widget* notification = notification_waiter.WaitIfNeededAndGet();
  views::test::WidgetDestroyedWaiter notification_closing_observer(
      notification);
  notification_closing_observer.Wait();

  // After the notification expires, the device gets locked.
  chrome_session_observer.WaitForSessionLocked();
}

// Tests the SecurityTokenSessionBehavior policy in the initial user session
// after that user has been created.
class SecurityTokenSessionBehaviorSamlTest : public SecurityTokenSamlTest {
 protected:
  SecurityTokenSessionBehaviorSamlTest() = default;
  SecurityTokenSessionBehaviorSamlTest(
      const SecurityTokenSessionBehaviorSamlTest&) = delete;
  SecurityTokenSessionBehaviorSamlTest& operator=(
      const SecurityTokenSessionBehaviorSamlTest&) = delete;
  ~SecurityTokenSessionBehaviorSamlTest() override = default;

  // Configures and installs the user session certificate provider extension.
  void PrepareUserCertificateProviderExtension(Profile* profile) {
    user_extension_mixin_.InitWithMockPolicyProvider(profile,
                                                     policy_provider());
    ASSERT_NO_FATAL_FAILURE(
        test_certificate_provider_extension_mixin_.ForceInstall(profile));
  }

  // Makes the user session extension call certificateProvider.setCertificates()
  // without providing any certificates, thus simulating the removal of a
  // security token.
  void SimulateSecurityTokenRemoval() {
    ASSERT_TRUE(user_certificate_provider_extension());
    user_certificate_provider_extension()->set_should_provide_certificates(
        false);
    user_certificate_provider_extension()->TriggerSetCertificates();
  }

  TestCertificateProviderExtension* user_certificate_provider_extension() {
    return test_certificate_provider_extension_mixin_.extension();
  }

 private:
  ExtensionForceInstallMixin user_extension_mixin_{&mixin_host_};
  TestCertificateProviderExtensionMixin
      test_certificate_provider_extension_mixin_{&mixin_host_,
                                                 &user_extension_mixin_};
};

// Tests the SecurityTokenSessionBehavior policy with value "LOGOUT".
IN_PROC_BROWSER_TEST_F(SecurityTokenSessionBehaviorSamlTest, Logout) {
  // Login
  StartSignIn();
  WaitForPinDialog();
  InputPinByClickingKeypad(GetCorrectPin());
  ClickPinDialogSubmit();
  test::WaitForPrimaryUserSessionStart();

  // Setup extension and pref.
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());
  PrepareUserCertificateProviderExtension(profile);
  g_browser_process->local_state()->SetString(
      prefs::kSecurityTokenSessionBehavior, "LOGOUT");

  // Removal of the certificate should lead to the end of the current session.
  ChromeSessionObserver chrome_session_observer;
  SimulateSecurityTokenRemoval();
  chrome_session_observer.WaitForChromeTerminating();
}

}  // namespace ash
