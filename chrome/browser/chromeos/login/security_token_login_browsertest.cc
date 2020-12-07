// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/challenge_response/known_user_pref_utils.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/features/simple_feature.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

using ash::LoginScreenTestApi;

namespace chromeos {

namespace {

// The PIN code that the test certificate provider extension is configured to
// expect.
constexpr char kCorrectPin[] = "17093";
constexpr char kWrongPin[] = "1234";

// UI golden strings in the en-US locale:
constexpr char kChallengeResponseLoginLabel[] = "Sign in with smart card";
constexpr char kChallengeResponseErrorLabel[] =
    "Couldn’t recognize your smart card. Try again.";
constexpr char kPinDialogDefaultTitle[] = "Smart card PIN";
constexpr char kPinDialogInvalidPinTitle[] = "Invalid PIN.";
constexpr char kPinDialogInvalidPin2AttemptsTitle[] =
    "Invalid PIN. 2 attempts left";
// TODO(crbug.com/1060695): Fix the incorrect plural in the message.
constexpr char kPinDialogInvalidPin1AttemptTitle[] =
    "Invalid PIN. 1 attempts left";
constexpr char kPinDialogNoAttemptsLeftTitle[] =
    "Maximum allowed attempts exceeded.";

constexpr char kChallengeData[] = "challenge";

// Returns the profile into which login-screen extensions are force-installed.
Profile* GetOriginalSigninProfile() {
  return chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

// Custom implementation of the CryptohomeClient that triggers the
// challenge-response protocol when authenticating the user.
class ChallengeResponseFakeCryptohomeClient : public FakeCryptohomeClient {
 public:
  ChallengeResponseFakeCryptohomeClient() = default;
  ChallengeResponseFakeCryptohomeClient(
      const ChallengeResponseFakeCryptohomeClient&) = delete;
  ChallengeResponseFakeCryptohomeClient& operator=(
      const ChallengeResponseFakeCryptohomeClient&) = delete;
  ~ChallengeResponseFakeCryptohomeClient() override = default;

  void set_challenge_response_account_id(const AccountId& account_id) {
    challenge_response_account_id_ = account_id;
  }

  void MountEx(const cryptohome::AccountIdentifier& cryptohome_id,
               const cryptohome::AuthorizationRequest& auth,
               const cryptohome::MountRequest& request,
               DBusMethodCallback<cryptohome::BaseReply> callback) override {
    CertificateProviderService* certificate_provider_service =
        CertificateProviderServiceFactory::GetForBrowserContext(
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
        base::BindOnce(&ChallengeResponseFakeCryptohomeClient::
                           ContinueMountExWithSignature,
                       base::Unretained(this), cryptohome_id,
                       std::move(callback)));
  }

 private:
  void ContinueMountExWithSignature(
      const cryptohome::AccountIdentifier& cryptohome_id,
      DBusMethodCallback<cryptohome::BaseReply> callback,
      net::Error error,
      const std::vector<uint8_t>& signature) {
    cryptohome::BaseReply reply;
    cryptohome::MountReply* mount =
        reply.MutableExtension(cryptohome::MountReply::reply);
    mount->set_sanitized_username(GetStubSanitizedUsername(cryptohome_id));
    if (error != net::OK || signature.empty())
      reply.set_error(cryptohome::CRYPTOHOME_ERROR_MOUNT_FATAL);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), reply));
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

}  // namespace

// Tests the challenge-response based login (e.g., using a smart card) for an
// existing user.
class SecurityTokenLoginTest : public MixinBasedInProcessBrowserTest,
                               public LocalStateMixin::Delegate {
 protected:
  SecurityTokenLoginTest()
      : cryptohome_client_(new ChallengeResponseFakeCryptohomeClient) {
    // Don't shut down when no browser is open, since it breaks the test and
    // since it's not the real Chrome OS behavior.
    set_exit_when_last_browser_closes(false);

    login_manager_mixin_.AppendManagedUsers(1);
    cryptohome_client_->set_challenge_response_account_id(
        GetChallengeResponseAccountId());
  }

  SecurityTokenLoginTest(const SecurityTokenLoginTest&) = delete;
  SecurityTokenLoginTest& operator=(const SecurityTokenLoginTest&) = delete;
  ~SecurityTokenLoginTest() override = default;

  // MixinBasedInProcessBrowserTest:

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);

    // Avoid aborting the user sign-in due to the user policy requests not being
    // faked in the test.
    command_line->AppendSwitch(
        chromeos::switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    WaitForLoginScreenWidgetShown();
  }

  void TearDownOnMainThread() override {
    certificate_provider_extension_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  // LocalStateMixin::Delegate:

  void SetUpLocalState() override { RegisterChallengeResponseKey(); }

  AccountId GetChallengeResponseAccountId() const {
    return login_manager_mixin_.users()[0].account_id;
  }

  TestCertificateProviderExtension* certificate_provider_extension() {
    return certificate_provider_extension_.get();
  }

  void StartLoginAndWaitForPinDialog() {
    base::RunLoop pin_dialog_waiting_run_loop;
    LoginScreenTestApi::SetPinRequestWidgetShownCallback(
        pin_dialog_waiting_run_loop.QuitClosure());
    LoginScreenTestApi::ClickChallengeResponseButton(
        GetChallengeResponseAccountId());
    pin_dialog_waiting_run_loop.Run();
  }

  void WaitForChallengeResponseLabel(const std::string& awaited_label) {
    test::TestPredicateWaiter waiter(base::BindRepeating(
        [](const AccountId& account_id, const std::string& awaited_label) {
          return LoginScreenTestApi::GetChallengeResponseLabel(account_id) ==
                 base::UTF8ToUTF16(awaited_label);
        },
        GetChallengeResponseAccountId(), awaited_label));
    waiter.Wait();
  }

  void WaitForPinDialogTitle(const std::string& awaited_title) {
    test::TestPredicateWaiter waiter(base::BindRepeating(
        [](const std::string& awaited_title) {
          return LoginScreenTestApi::GetPinRequestWidgetTitle() ==
                 base::UTF8ToUTF16(awaited_title);
        },
        awaited_title));
    waiter.Wait();
  }

  void WaitForActiveSession() { login_manager_mixin_.WaitForActiveSession(); }

  // Configures and installs the test certificate provider extension.
  void PrepareCertificateProviderExtension() {
    certificate_provider_extension_ =
        std::make_unique<TestCertificateProviderExtension>(
            GetOriginalSigninProfile());
    certificate_provider_extension_->set_require_pin(kCorrectPin);
    extension_force_install_mixin_.InitWithDeviceStateMixin(
        GetOriginalSigninProfile(), &device_state_mixin_);
    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
        TestCertificateProviderExtension::GetExtensionSourcePath(),
        TestCertificateProviderExtension::GetExtensionPemPath(),
        ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad));
  }

 private:
  void RegisterChallengeResponseKey() {
    // The global user manager is not created until after the Local State is
    // initialized, but in order for the user_manager::known_user:: methods to
    // work we create a temporary instance of the user manager here.
    auto user_manager = std::make_unique<user_manager::FakeUserManager>();
    user_manager->set_local_state(g_browser_process->local_state());
    user_manager::ScopedUserManager scoper(std::move(user_manager));

    ChallengeResponseKey challenge_response_key;
    challenge_response_key.set_public_key_spki_der(
        TestCertificateProviderExtension::GetCertificateSpki());
    challenge_response_key.set_extension_id(
        TestCertificateProviderExtension::extension_id());

    base::Value challenge_response_keys_value =
        SerializeChallengeResponseKeysForKnownUser({challenge_response_key});
    user_manager::known_user::SetChallengeResponseKeys(
        GetChallengeResponseAccountId(),
        std::move(challenge_response_keys_value));
  }

  void WaitForLoginScreenWidgetShown() {
    base::RunLoop run_loop;
    LoginScreenTestApi::AddOnLockScreenShownCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Bypass "signin_screen" feature only enabled for allowlisted extensions.
  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest
      feature_allowlist_{TestCertificateProviderExtension::extension_id()};

  // Unowned (referencing a global singleton)
  ChallengeResponseFakeCryptohomeClient* const cryptohome_client_;
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};

  std::unique_ptr<TestCertificateProviderExtension>
      certificate_provider_extension_;
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
            base::UTF8ToUTF16(kChallengeResponseLoginLabel));

  // The challenge-response "start" button is clicked. The MountEx request is
  // sent to cryptohome, and in turn cryptohome makes a challenge request. The
  // certificate provider extension receives this request and requests the PIN
  // dialog.
  StartLoginAndWaitForPinDialog();
  EXPECT_EQ(LoginScreenTestApi::GetPinRequestWidgetTitle(),
            base::UTF8ToUTF16(kPinDialogDefaultTitle));

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
            base::UTF8ToUTF16(kChallengeResponseLoginLabel));

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
            base::UTF8ToUTF16(kChallengeResponseLoginLabel));
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
            base::UTF8ToUTF16(kChallengeResponseErrorLabel));
}

}  // namespace chromeos
