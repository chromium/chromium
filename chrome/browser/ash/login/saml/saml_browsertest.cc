// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/callback_list.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "chrome/browser/ash/attestation/mock_machine_certificate_uploader.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/saml/fake_saml_idp_mixin.h"
#include "chrome/browser/ash/login/saml/lockscreen_reauth_dialog_test_helper.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/saml_challenge_key_handler.h"
#include "chrome/browser/ui/webui/ash/login/saml_confirm_password_handler.h"
#include "chrome/browser/ui/webui/ash/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/http_auth_dialog/http_auth_dialog.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

namespace em = ::enterprise_management;

using ::base::test::RunOnceCallback;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::WithArgs;

const test::UIPath kPasswordInput = {"saml-confirm-password", "passwordInput"};
const test::UIPath kPasswordConfirmInput = {"saml-confirm-password",
                                            "confirmPasswordInput"};
const test::UIPath kPasswordSubmit = {"saml-confirm-password", "next"};
const test::UIPath kSigninFrameDialog = {"gaia-signin", "signin-frame-dialog"};
const test::UIPath kSamlNoticeMessage = {"gaia-signin", "signin-frame-dialog",
                                         "saml-notice-message"};
const test::UIPath kSamlNoticeContainer = {"gaia-signin", "signin-frame-dialog",
                                           "saml-notice-container"};
constexpr test::UIPath kBackButton = {"gaia-signin", "signin-frame-dialog",
                                      "signin-back-button"};
constexpr test::UIPath kChangeIdPButton = {"gaia-signin", "signin-frame-dialog",
                                           "change-account"};
constexpr test::UIPath kEnterprisePrimaryButton = {
    "enterprise-enrollment", "step-signin", "primary-action-button"};
constexpr test::UIPath kSamlBackButton = {"gaia-signin", "signin-frame-dialog",
                                          "saml-back-button"};
const test::UIPath kGaiaLoading = {"gaia-signin", "step-loading"};
constexpr test::UIPath kFatalErrorActionButton = {"signin-fatal-error",
                                                  "actionButton"};
constexpr test::UIPath kErrorPage = {"main-frame-error"};

constexpr char kGAIASIDCookieName[] = "SID";
constexpr char kGAIALSIDCookieName[] = "LSID";

constexpr char kTestAuthSIDCookie1[] = "fake-auth-SID-cookie-1";
constexpr char kTestAuthSIDCookie2[] = "fake-auth-SID-cookie-2";
constexpr char kTestAuthLSIDCookie1[] = "fake-auth-LSID-cookie-1";
constexpr char kTestAuthLSIDCookie2[] = "fake-auth-LSID-cookie-2";

constexpr char kNonSAMLUserEmail[] = "frank@corp.example.com";

constexpr char kFirstSAMLUserGaiaId[] = "alice-gaia";
constexpr char kSecondSAMLUserGaiaId[] = "bob-gaia";
constexpr char kThirdSAMLUserGaiaId[] = "carol-gaia";
constexpr char kFifthSAMLUserGaiaId[] = "eve-gaia";
constexpr char kNonSAMLUserGaiaId[] = "frank-gaia";

constexpr char kAdditionalIdPHost[] = "login2.corp.example.com";

constexpr char kSAMLIdPCookieName[] = "saml";
constexpr char kSAMLIdPCookieValue1[] = "value-1";
constexpr char kSAMLIdPCookieValue2[] = "value-2";

constexpr char kTestUserinfoToken[] = "fake-userinfo-token";
constexpr char kTestRefreshToken[] = "fake-refresh-token";

constexpr char kAffiliationID[] = "some-affiliation-id";

constexpr char kDeviceTrustMatchHistogramName[] =
    "Enterprise.VerifiedAccess.SAML.DeviceTrustMatchesEndpoints";
constexpr char kDeviceTrustAttestationFunnelStep[] =
    "Enterprise.DeviceTrust.Attestation.Funnel";

constexpr char kSAMLLink[] = "link";
constexpr char kSAMLLinkedPageURLPattern[] =
    "*"
    "/linked";

// A FakeUserDataAuthClient that stores the salted and hashed secret passed
// to AddAuthFactor().
class SecretInterceptingFakeUserDataAuthClient : public FakeUserDataAuthClient {
 public:
  SecretInterceptingFakeUserDataAuthClient();

  SecretInterceptingFakeUserDataAuthClient(
      const SecretInterceptingFakeUserDataAuthClient&) = delete;
  SecretInterceptingFakeUserDataAuthClient& operator=(
      const SecretInterceptingFakeUserDataAuthClient&) = delete;

  void AddAuthFactor(const ::user_data_auth::AddAuthFactorRequest& request,
                     AddAuthFactorCallback callback) override;

  const std::string& salted_hashed_secret() { return salted_hashed_secret_; }

 private:
  std::string salted_hashed_secret_;
};

SecretInterceptingFakeUserDataAuthClient::
    SecretInterceptingFakeUserDataAuthClient() = default;

void SecretInterceptingFakeUserDataAuthClient::AddAuthFactor(
    const ::user_data_auth::AddAuthFactorRequest& request,
    AddAuthFactorCallback callback) {
  CHECK(request.auth_factor().type() ==
        user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);
  salted_hashed_secret_ = request.auth_input().password_input().secret();
  FakeUserDataAuthClient::AddAuthFactor(request, std::move(callback));
}

// Gaia path used by Gaia screen in the current sign-in attempt.
WizardContext::GaiaPath GaiaPath() {
  return LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->gaia_config.gaia_path;
}

}  // namespace

class SamlTestBase : public OobeBaseTest {
 public:
  SamlTestBase() {
    auto cryptohome_client =
        std::make_unique<SecretInterceptingFakeUserDataAuthClient>();
    cryptohome_client_ = cryptohome_client.get();
    FakeUserDataAuthClient::TestApi::OverrideGlobalInstance(
        std::move(cryptohome_client));

    fake_gaia_.set_initialize_configuration(false);
  }

  SamlTestBase(const SamlTestBase&) = delete;
  SamlTestBase& operator=(const SamlTestBase&) = delete;

  ~SamlTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);

    // TODO(crbug.com/1177416) - Fix this with a proper SSL solution.
    command_line->AppendSwitch(::switches::kIgnoreCertificateErrors);

    // This will change the verification key to be used by the
    // CloudPolicyValidator. It will allow for the policy provided by the
    // PolicyBuilder to pass the signature validation.
    command_line->AppendSwitchASCII(
        policy::switches::kPolicyVerificationKey,
        policy::PolicyBuilder::GetEncodedPolicyVerificationKey());
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    OobeBaseTest::TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    // Without this tests are flaky. TODO(b/333450354): Determine why and fix.
    SetDelayNetworkCallsForTesting(true);

    // Allowlist the default EMK to sign enterprise challenge.
    ::attestation::SignEnterpriseChallengeRequest
        sign_enterprise_challenge_request;
    sign_enterprise_challenge_request.set_username("");
    sign_enterprise_challenge_request.set_key_label(
        attestation::kEnterpriseMachineKey);
    sign_enterprise_challenge_request.set_device_id("device_id");
    sign_enterprise_challenge_request.set_include_customer_id(true);
    AttestationClient::Get()
        ->GetTestInterface()
        ->AllowlistSignEnterpriseChallengeKey(
            sign_enterprise_challenge_request);

    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kFirstUserCorpExampleComEmail,
        fake_saml_idp()->GetSamlPageUrl());
    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kSecondUserCorpExampleComEmail,
        fake_saml_idp()->GetSamlPageUrl());
    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kThirdUserCorpExampleComEmail,
        fake_saml_idp()->GetHttpSamlPageUrl());
    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kFourthUserCorpExampleTestEmail,
        fake_saml_idp()->GetSamlWithDeviceAttestationUrl());
    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kFifthUserExampleTestEmail,
        fake_saml_idp()->GetSamlPageUrl());
    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kSixthUserCorpExampleTestEmail,
        fake_saml_idp()->GetSamlWithDeviceTrustUrl());

    fake_gaia_.fake_gaia()->SetConfigurationHelper(
        saml_test_users::kFirstUserCorpExampleComEmail, kTestAuthSIDCookie1,
        kTestAuthLSIDCookie1);

    OobeBaseTest::SetUpOnMainThread();
  }

  void SetupAuthFlowChangeListener() {
    content::ExecuteScriptAsync(
        GetLoginUI()->GetWebContents(),
        "$('gaia-signin').authenticator.addEventListener('authFlowChange',"
        "    function f() {"
        "      $('gaia-signin').authenticator.removeEventListener("
        "          'authFlowChange', f);"
        "      window.domAutomationController.send("
        "          $('gaia-signin').isSamlAuthFlowForTesting() ?"
        "              'SamlLoaded' : 'GaiaLoaded');"
        "    });");
  }

  virtual void StartSamlAndWaitForIdpPageLoad(const std::string& gaia_email) {
    OobeScreenWaiter(GetFirstSigninScreen()).Wait();

    content::DOMMessageQueue message_queue(
        GetLoginUI()->GetWebContents());  // Start observe before SAML.
    SetupAuthFlowChangeListener();
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(gaia_email, "", "[]");

    std::string message;
    do {
      ASSERT_TRUE(message_queue.WaitForMessage(&message));
    } while (message != "\"SamlLoaded\"");
  }

  void SendConfirmPassword(const std::string& password_to_confirm) {
    test::OobeJS().TypeIntoPath(password_to_confirm, kPasswordInput);
    test::OobeJS().TapOnPath(kPasswordSubmit);
  }

  void SetManualPasswords(const std::string& password,
                          const std::string& confirm_password) {
    test::OobeJS().TypeIntoPath(password, kPasswordInput);
    test::OobeJS().TypeIntoPath(confirm_password, kPasswordConfirmInput);
    test::OobeJS().TapOnPath(kPasswordSubmit);
  }

  void ExpectFatalErrorMessage(const std::string& error_message) {
    OobeScreenWaiter(SignInFatalErrorView::kScreenId).Wait();

    EXPECT_TRUE(LoginScreenTestApi::IsShutdownButtonShown());
    EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
    EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

    test::OobeJS().ExpectElementText(error_message,
                                     {"signin-fatal-error", "subtitle"});
  }

  FakeSamlIdpMixin* fake_saml_idp() { return &fake_saml_idp_mixin_; }

 protected:
  raw_ptr<SecretInterceptingFakeUserDataAuthClient, DanglingUntriaged>
      cryptohome_client_;

  FakeGaiaMixin fake_gaia_{&mixin_host_};
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

 private:
  FakeSamlIdpMixin fake_saml_idp_mixin_{&mixin_host_, &fake_gaia_};
};

// This test is run with both variants of CheckPasswordsAgainstCryptohomeHelper
// feature.
class SamlTestWithFeatures : public SamlTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  SamlTestWithFeatures() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam()) {
      enabled_features.push_back(
          features::kCheckPasswordsAgainstCryptohomeHelper);
    } else {
      disabled_features.push_back(
          features::kCheckPasswordsAgainstCryptohomeHelper);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ImprovedScrapingTestBase : public SamlTestBase {
 public:
  ImprovedScrapingTestBase();

 protected:
  void SetFeatures(bool enable_improved_scraping);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

ImprovedScrapingTestBase::ImprovedScrapingTestBase() = default;

void ImprovedScrapingTestBase::SetFeatures(bool enable_improved_scraping) {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  if (enable_improved_scraping) {
    enabled_features.push_back(
        features::kCheckPasswordsAgainstCryptohomeHelper);
  } else {
    disabled_features.push_back(
        features::kCheckPasswordsAgainstCryptohomeHelper);
  }
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

// Saml test with kCheckPasswordsAgainstCryptohomeHelper enabled.
class SamlTestWithImprovedScraping : public ImprovedScrapingTestBase {
 public:
  SamlTestWithImprovedScraping() {
    SetFeatures(/*enable_improved_scraping=*/true);
  }
};

// Tests that signin frame should display the SAML notice and the 'back' button

// Saml test with kCheckPasswordsAgainstCryptohomeHelper disabled.
class SamlTestWithoutImprovedScraping : public ImprovedScrapingTestBase {
 public:
  SamlTestWithoutImprovedScraping() {
    SetFeatures(/*enable_improved_scraping=*/false);
  }
};
// when SAML IdP page is loaded. And the 'back' button goes back to gaia on
// clicking.
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures, SamlUI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Saml flow UI expectations.
  test::OobeJS().ExpectVisiblePath(kSamlNoticeContainer);
  test::OobeJS().ExpectVisiblePath(kBackButton);
  std::string js = "$SamlNoticeMessagePath.textContent.indexOf('$Host') > -1";
  base::ReplaceSubstringsAfterOffset(
      &js, 0, "$SamlNoticeMessagePath",
      test::GetOobeElementPath(kSamlNoticeMessage));
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Host",
                                     fake_saml_idp()->GetIdpHost());
  test::OobeJS().ExpectTrue(js);

  content::DOMMessageQueue message_queue(GetLoginUI()->GetWebContents());
  SetupAuthFlowChangeListener();
  // Click on 'back'.
  test::OobeJS().ClickOnPath(kSamlBackButton);

  // Auth flow should change back to Gaia.
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"GaiaLoaded\"");

  // Saml flow is gone.
  test::OobeJS().ExpectHiddenPath(kSamlNoticeContainer);
}

// This test is run with both new API Create Account enable or disable.
using SamlWithCreateAccountAPITestParams = std::tuple<bool, bool>;
class SamlWithCreateAccountAPITest
    : public SamlTestBase,
      public testing::WithParamInterface<SamlWithCreateAccountAPITestParams> {
 public:
  SamlWithCreateAccountAPITest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    // Handle Gaia's create account message
    if (IsRecordCreateAccountFeatureEnabled()) {
      enabled_features.push_back(features::kGaiaRecordAccountCreation);
    } else {
      disabled_features.push_back(features::kGaiaRecordAccountCreation);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  bool IsRecordCreateAccountFeatureEnabled() const {
    return std::get<0>(GetParam());
  }
  bool IsNewAccountSignedUp() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The SAML IdP requires HTTP Protocol-level authentication (Basic in this
// case).
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures, IdpRequiresHttpAuth) {
  fake_saml_idp()->SetRequireHttpBasicAuth(true);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");

  // This is not calling StartSamlAndWaitForIdpPageLoad because it has
  // to wait for the auth credentials entry dialog in between. Also, only load
  // the gaia page first so we can get a pointer to the gaia frame's
  // WebContents.
  WaitForGaiaPageLoad();

  extensions::WebViewGuest* gaia_guest = signin::GetAuthWebViewGuest(
      GetLoginUI()->GetWebContents(), gaia_frame_parent_);
  ASSERT_TRUE(gaia_guest);

  // Start observing before initiating SAML sign-in.
  content::DOMMessageQueue message_queue(GetLoginUI()->GetWebContents());

  SetupAuthFlowChangeListener();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(saml_test_users::kFirstUserCorpExampleComEmail,
                                "", "[]");

  ASSERT_TRUE(base::test::RunUntil(
      []() { return HttpAuthDialog::GetAllDialogsForTest().size() == 1; }));
  // Note that the actual credentials don't matter because `fake_saml_idp()`
  // doesn't check those (only that something has been provided).
  HttpAuthDialog::GetAllDialogsForTest().front()->SupplyCredentialsForTest(
      u"user", u"pwd");

  // Now the SAML sign-in form should actually load.
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"SamlLoaded\"");

  test::OobeJS().ExpectPathDisplayed(false, kBackButton);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("not_the_password", {"Dummy"});
  SigninFrameJS().TypeIntoPath("actual_password", {"Password"});

  SigninFrameJS().TapOn("Submit");

  // Login should finish login and a session should start.
  test::WaitForPrimaryUserSessionStart();

  // We should no longer be using the ash http auth dialog.
  EXPECT_FALSE(HttpAuthDialog::IsEnabled());
}

// Tests the sign-in flow when the credentials passing API is used.
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures, CredentialPassingAPI) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("not_the_password", {"Dummy"});
  SigninFrameJS().TypeIntoPath("actual_password", {"Password"});

  SigninFrameJS().TapOn("Submit");

  // Login should finish login and a session should start.
  test::WaitForPrimaryUserSessionStart();

  // Regression test for http://crbug.com/490737: Verify that the user's actual
  // password was used, not the contents of the first type=password input field
  // found on the page.
  Key key("actual_password");
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                SystemSaltGetter::ConvertRawSaltToHexString(
                    FakeCryptohomeMiscClient::GetStubSystemSalt()));
  EXPECT_EQ(key.GetSecret(), cryptohome_client_->salted_hashed_secret());

  EXPECT_TRUE(user_manager::KnownUser(g_browser_process->local_state())
                  .GetIsUsingSAMLPrincipalsAPI(AccountId::FromUserEmailGaiaId(
                      saml_test_users::kFirstUserCorpExampleComEmail,
                      kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 1, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Provider", 1, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 1,
                                     1);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 1,
                                     1);
}

// Tests the sign-in flow when the credentials passing API is used w/o 'confirm'
// call. The password from the last 'add' should be used.
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures,
                       CredentialPassingAPIWithoutConfirm) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate(
      "saml_api_login_auth_without_confirm.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("last_password", {"Dummy"});
  SigninFrameJS().TypeIntoPath("not_confirmed_password", {"Password"});

  SigninFrameJS().TapOn("Submit");

  // Login should finish login and a session should start.
  test::WaitForPrimaryUserSessionStart();

  // Verify that last password sent by 'add' used.
  Key key("last_password");
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                SystemSaltGetter::ConvertRawSaltToHexString(
                    FakeCryptohomeMiscClient::GetStubSystemSalt()));
  EXPECT_EQ(key.GetSecret(), cryptohome_client_->salted_hashed_secret());

  EXPECT_TRUE(user_manager::KnownUser(g_browser_process->local_state())
                  .GetIsUsingSAMLPrincipalsAPI(AccountId::FromUserEmailGaiaId(
                      saml_test_users::kFirstUserCorpExampleComEmail,
                      kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 1, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Provider", 1, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 1,
                                     1);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 1,
                                     1);
}

// Tests the single password scraped flow.
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures, ScrapedSingle) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  content::DOMMessageQueue message_queue(GetLoginUI()->GetWebContents());
  // Make sure that the password is scraped correctly.
  ASSERT_TRUE(content::ExecJs(
      GetLoginUI()->GetWebContents(),
      "$('gaia-signin').authenticator.addEventListener('authCompleted',"
      "    function(e) {"
      "      var password = e.detail.password;"
      "      window.domAutomationController.send(password);"
      "    });"));

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  // Scraping a single password should finish the login and start the session.
  SigninFrameJS().TapOn("Submit");
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"fake_password\"");

  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::KnownUser(g_browser_process->local_state())
                   .GetIsUsingSAMLPrincipalsAPI(AccountId::FromUserEmailGaiaId(
                       saml_test_users::kFirstUserCorpExampleComEmail,
                       kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      1, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 1,
                                     1);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 1,
                                     1);
}

// Tests password scraping from a dynamically created password field.
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures, ScrapedDynamic) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().Evaluate(
      "(function() {"
      "  var newPassInput = document.createElement('input');"
      "  newPassInput.id = 'DynamicallyCreatedPassword';"
      "  newPassInput.type = 'password';"
      "  newPassInput.name = 'Password';"
      "  document.forms[0].appendChild(newPassInput);"
      "})();");

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"DynamicallyCreatedPassword"});

  // Scraping a single password should finish the login and start the session.
  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::KnownUser(g_browser_process->local_state())
                   .GetIsUsingSAMLPrincipalsAPI(AccountId::FromUserEmailGaiaId(
                       saml_test_users::kFirstUserCorpExampleComEmail,
                       kFirstSAMLUserGaiaId)));
}

// Tests the multiple password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTestWithoutImprovedScraping, ScrapedMultiple) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TypeIntoPath("password1", {"Password1"});
  SigninFrameJS().TapOn("Submit");
  // Lands on confirm password screen.
  OobeScreenWaiter(SamlConfirmPasswordView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  // Entering an unknown password should go back to the confirm password screen.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(SamlConfirmPasswordView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  // Either scraped password should be able to sign-in.
  SendConfirmPassword("password1");
  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::KnownUser(g_browser_process->local_state())
                   .GetIsUsingSAMLPrincipalsAPI(AccountId::FromUserEmailGaiaId(
                       saml_test_users::kFirstUserCorpExampleComEmail,
                       kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      2, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 1,
                                     1);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 1,
                                     1);
}

// Tests the multiple password scraped flow.
// TODO(crbug.com/40214270): This feature was deprioritized in mid-2022.
// Restore test once work resumes, or feel free to clean it up if you
// step on it past mid-2024.
IN_PROC_BROWSER_TEST_F(SamlTestWithImprovedScraping, DISABLED_ScrapedMultiple) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TypeIntoPath("password1", {"Password1"});
  SigninFrameJS().TapOn("Submit");

  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::KnownUser(g_browser_process->local_state())
                   .GetIsUsingSAMLPrincipalsAPI(AccountId::FromUserEmailGaiaId(
                       saml_test_users::kFirstUserCorpExampleComEmail,
                       kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      2, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 1,
                                     1);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 1,
                                     1);
}

IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures, ScrapedNone) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_no_passwords.html");

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TapOn("Submit");

  // Lands on confirm password screen with manual input state.
  OobeScreenWaiter(SamlConfirmPasswordView::kScreenId).Wait();
  test::OobeJS().ExpectTrue("$('saml-confirm-password').isManualInput");
  // Entering passwords that don't match will make us land again in the same
  // page.
  SetManualPasswords("Test1", "Test2");
  OobeScreenWaiter(SamlConfirmPasswordView::kScreenId).Wait();
  test::OobeJS().ExpectTrue("$('saml-confirm-password').isManualInput");

  // Two matching passwords should let the user to sign in.
  SetManualPasswords("Test1", "Test1");
  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::KnownUser(g_browser_process->local_state())
                   .GetIsUsingSAMLPrincipalsAPI(AccountId::FromUserEmailGaiaId(
                       saml_test_users::kFirstUserCorpExampleComEmail,
                       kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      0, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 1,
                                     1);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 1,
                                     1);
}

// Types the second user e-mail into the GAIA login form but then authenticates
// as the first user via SAML. Verifies that the logged-in user is correctly
// identified as the first user.
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures, UseAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  // Type the second user e-mail into the GAIA login form.
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSecondUserCorpExampleComEmail);

  // Authenticate as the first user via SAML (the `Email` provided here is
  // irrelevant - the authenticated user's e-mail address that FakeGAIA reports
  // was set via `SetConfigurationHelper`).
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  ASSERT_TRUE(user);
  EXPECT_EQ(saml_test_users::kFirstUserCorpExampleComEmail,
            user->GetAccountId().GetUserEmail());
}

// Verifies that if the authenticated user's e-mail address cannot be retrieved,
// an error message is shown.
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures,
                       FailToRetrieveAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  fake_gaia_.fake_gaia()->SetConfigurationHelper("", kTestAuthSIDCookie1,
                                                 kTestAuthLSIDCookie1);
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  ExpectFatalErrorMessage(
      l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS));

  test::OobeJS().TapOnPath(kFatalErrorActionButton);
  WaitForSigninScreen();
}

IN_PROC_BROWSER_TEST_F(SamlTestWithoutImprovedScraping, PasswordConfirmFlow) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TypeIntoPath("password1", {"Password1"});
  SigninFrameJS().TapOn("Submit");

  // Lands on confirm password screen with no error message.
  OobeScreenWaiter(SamlConfirmPasswordView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  test::OobeJS().ExpectTrue(
      "!$('saml-confirm-password').$.passwordInput.invalid");

  // Enter an unknown password for the first time should go back to confirm
  // password screen with error message.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(SamlConfirmPasswordView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  test::OobeJS().ExpectTrue(
      "$('saml-confirm-password').$.passwordInput.invalid");

  // Enter an unknown password 2nd time should go back fatal error message.
  SendConfirmPassword("wrong_password");
  ExpectFatalErrorMessage(
      l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_PASSWORD_VERIFICATION));

  test::OobeJS().TapOnPath(kFatalErrorActionButton);
  WaitForSigninScreen();

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      2, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 1,
                                     1);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 1,
                                     1);
}

// Verifies that when the login flow redirects from one host to another, the
// notice shown to the user is updated. This guards against regressions of
// http://crbug.com/447818.
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures, NoticeUpdatedOnRedirect) {
  // Start another https server at `kAdditionalIdPHost`.
  net::EmbeddedTestServer saml_https_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig saml_cert_config;
  saml_cert_config.dns_names = {kAdditionalIdPHost};
  saml_https_server.SetSSLConfig(saml_cert_config);
  saml_https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(saml_https_server.Start());

  // Make the login flow redirect to `kAdditionalIdPHost`.
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_instant_meta_refresh.html");
  fake_saml_idp()->SetRefreshURL(
      saml_https_server.GetURL(kAdditionalIdPHost, "/simple.html"));
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Wait until the notice shown to the user is updated to contain
  // `kAdditionalIdPHost`.
  std::string js =
      "new Promise(resolve => {"
      "  var sendIfHostFound = function() {"
      "    var found = $SamlNoticeMessagePath.textContent.indexOf('$Host') > "
      "-1;"
      "    if (found)"
      "      resolve(true);"
      "    return found;"
      "  };"
      "  var processEventsAndSendIfHostFound = function() {"
      "    window.setTimeout(function() {"
      "      if (sendIfHostFound()) {"
      "        $('gaia-signin').authenticator.removeEventListener("
      "            'authDomainChange',"
      "            processEventsAndSendIfHostFound);"
      "      }"
      "    }, 0);"
      "  };"
      "  if (!sendIfHostFound()) {"
      "    $('gaia-signin').authenticator.addEventListener("
      "        'authDomainChange',"
      "        processEventsAndSendIfHostFound);"
      "  }"
      "});";
  base::ReplaceSubstringsAfterOffset(
      &js, 0, "$SamlNoticeMessagePath",
      test::GetOobeElementPath(kSamlNoticeMessage));
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Host", kAdditionalIdPHost);
  EXPECT_EQ(true, content::ExecJs(GetLoginUI()->GetWebContents(), js));

  // Verify that the notice is visible.
  test::OobeJS().ExpectVisiblePath(kSamlNoticeContainer);
}

// Verifies that when GAIA attempts to redirect to a SAML IdP served over http,
// not https, the redirect is blocked and an error message is shown.
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures, HTTPRedirectDisallowed) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  WaitForSigninScreen();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(saml_test_users::kThirdUserCorpExampleComEmail,
                                "", "[]");

  const GURL url = fake_saml_idp()->GetHttpSamlPageUrl();
  ExpectFatalErrorMessage(l10n_util::GetStringFUTF8(
      IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL, base::UTF8ToUTF16(url.spec())));

  test::OobeJS().TapOnPath(kFatalErrorActionButton);
  WaitForSigninScreen();
}

// Verifies that when GAIA attempts to redirect to a page served over http, not
// https, via an HTML meta refresh, the redirect is blocked and an error message
// is shown. This guards against regressions of http://crbug.com/359515.
IN_PROC_BROWSER_TEST_P(SamlTestWithFeatures, MetaRefreshToHTTPDisallowed) {
  const GURL url = embedded_test_server()->base_url().Resolve("/SSO");
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_instant_meta_refresh.html");
  fake_saml_idp()->SetRefreshURL(url);

  WaitForSigninScreen();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(saml_test_users::kFirstUserCorpExampleComEmail,
                                "", "[]");

  ExpectFatalErrorMessage(l10n_util::GetStringFUTF8(
      IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL, base::UTF8ToUTF16(url.spec())));

  test::OobeJS().TapOnPath(kFatalErrorActionButton);
  WaitForSigninScreen();
}

// Tests the sign-in flow in Oobe when the credentials passing
// API is used in case of new account and existing account when
// the handle account creation message feature flag is enabled
// or disabled.
IN_PROC_BROWSER_TEST_P(SamlWithCreateAccountAPITest,
                       CredentialPassingAPIWithNewAccount) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  std::string login_auth_template = "saml_api_login_auth.html";
  if (IsNewAccountSignedUp()) {
    login_auth_template = "saml_api_login_auth_with_new_account.html";
  }
  fake_saml_idp()->SetLoginAuthHTMLTemplate(login_auth_template);
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("not_the_password", {"Dummy"});
  SigninFrameJS().TypeIntoPath("actual_password", {"Password"});

  SigninFrameJS().TapOn("Submit");

  // Login should finish login and a session should start.
  test::WaitForPrimaryUserSessionStart();

  // TODO (b/308176681): add test case for first user/non first user on the
  // device.
  if (IsRecordCreateAccountFeatureEnabled()) {
    if (IsNewAccountSignedUp()) {
      histogram_tester.ExpectUniqueSample(
          "ChromeOS.Gaia.CreateAccount.IsFirstUser", 1, 1);
      histogram_tester.ExpectUniqueSample("ChromeOS.Gaia.Done.Oobe.NewAccount",
                                          1, 1);
    } else {
      histogram_tester.ExpectTotalCount(
          "ChromeOS.Gaia.CreateAccount.IsFirstUser", 0);
      histogram_tester.ExpectUniqueSample("ChromeOS.Gaia.Done.Oobe.NewAccount",
                                          0, 1);
    }
  } else {
    histogram_tester.ExpectTotalCount("ChromeOS.Gaia.CreateAccount.IsFirstUser",
                                      0);
    histogram_tester.ExpectTotalCount("ChromeOS.Gaia.Done.Oobe.NewAccount", 0);
  }

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 1, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Provider", 1, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.UserInfo", 1,
                                     1);

  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 0,
                                     0);
  histogram_tester.ExpectBucketCount("ChromeOS.Gaia.Message.Saml.CloseView", 1,
                                     1);
}

class SAMLEnrollmentTest : public SamlTestBase {
 public:
  SAMLEnrollmentTest();

  SAMLEnrollmentTest(const SAMLEnrollmentTest&) = delete;
  SAMLEnrollmentTest& operator=(const SAMLEnrollmentTest&) = delete;

  ~SAMLEnrollmentTest() override;

  // SamlTest:
  void SetUpOnMainThread() override;
  void StartSamlAndWaitForIdpPageLoad(const std::string& gaia_email) override;

 protected:
  EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
};

SAMLEnrollmentTest::SAMLEnrollmentTest() {
  gaia_frame_parent_ = "authView";
  authenticator_id_ = "$('enterprise-enrollment').authenticator";
}

SAMLEnrollmentTest::~SAMLEnrollmentTest() = default;

void SAMLEnrollmentTest::SetUpOnMainThread() {
  FakeGaia::AccessTokenInfo token_info;
  token_info.token = kTestUserinfoToken;
  token_info.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  token_info.scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
  token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  token_info.email = saml_test_users::kFirstUserCorpExampleComEmail;
  fake_gaia_.fake_gaia()->IssueOAuthToken(kTestRefreshToken, token_info);

  SamlTestBase::SetUpOnMainThread();
}

void SAMLEnrollmentTest::StartSamlAndWaitForIdpPageLoad(
    const std::string& gaia_email) {
  LoginDisplayHost::default_host()->StartWizard(
      EnrollmentScreenView::kScreenId);
  WaitForGaiaPageBackButtonUpdate();
  auto flow_change_waiter =
      OobeBaseTest::CreateGaiaPageEventWaiter("authFlowChange");
  SigninFrameJS().TypeIntoPath(gaia_email, FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kEnterprisePrimaryButton);
  flow_change_waiter->Wait();
}

IN_PROC_BROWSER_TEST_F(SAMLEnrollmentTest, WithoutCredentialsPassingAPI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
}

IN_PROC_BROWSER_TEST_F(SAMLEnrollmentTest, WithCredentialsPassingAPI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
}

class SAMLPolicyTest : public SamlTestBase {
 public:
  SAMLPolicyTest();

  SAMLPolicyTest(const SAMLPolicyTest&) = delete;
  SAMLPolicyTest& operator=(const SAMLPolicyTest&) = delete;

  ~SAMLPolicyTest() override;

  // SamlTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  void SetSAMLOfflineSigninTimeLimitPolicy(int limit);
  void EnableTransferSAMLCookiesPolicy();
  void SetLoginBehaviorPolicyToSAMLInterstitial();
  void SetLoginVideoCaptureAllowedUrls(const std::vector<GURL>& allowed);
  void SetPolicyToHideUserPods();
  // SSO_profile in device policy blob is responsible for per-OU IdP
  // configuration.
  void SetSSOProfile(const std::string& sso_profile);

  void ShowGAIALoginForm();
  void ShowSAMLLoginForm();
  void MaybeWaitForSAMLToLoad();
  void ClickBackOnSAMLPage();
  void LogInWithSAML(const std::string& user_id,
                     const std::string& auth_sid_cookie,
                     const std::string& auth_lsid_cookie);
  void AddSamlUserWithZeroOfflineTimeLimit();

  std::string GetCookieValue(const std::string& name);

  void GetCookies(Profile* profile = nullptr);

 protected:
  NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  net::CookieList cookie_list_;

  // Add a fake user so the login screen does not show GAIA auth by default.
  // This enables tests to control when the GAIA is shown (and ensure it's
  // loaded after SAML config has been set up).
  LoginManagerMixin login_manager_{
      &mixin_host_,
      {LoginManagerMixin::TestUserInfo(
          AccountId::FromUserEmailGaiaId("user@gmail.com", "1111"))}};
};

SAMLPolicyTest::SAMLPolicyTest() {
  device_state_.SetState(
      DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  device_state_.set_skip_initial_policy_setup(true);
}

SAMLPolicyTest::~SAMLPolicyTest() = default;

void SAMLPolicyTest::SetUpInProcessBrowserTestFixture() {
  SessionManagerClient::InitializeFakeInMemory();

  SamlTestBase::SetUpInProcessBrowserTestFixture();

  // Add device affiliation id.
  std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
      device_state_.RequestDevicePolicyUpdate();
  device_policy_update->policy_data()->add_device_affiliation_ids(
      kAffiliationID);

  // Initialize user policy.
  provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                              /*is_first_policy_load_complete_return=*/true);
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void SAMLPolicyTest::SetUpOnMainThread() {
  SamlTestBase::SetUpOnMainThread();

  // Pretend that the test users' OAuth tokens are valid.
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail, kFirstSAMLUserGaiaId),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmailGaiaId(kNonSAMLUserEmail, kNonSAMLUserGaiaId),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFifthUserExampleTestEmail, kFifthSAMLUserGaiaId),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

  // Give affiliated users appropriate affiliation IDs.
  auto* user_manager = user_manager::UserManager::Get();
  user_manager->SetUserAffiliated(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail, kFirstSAMLUserGaiaId),
      /*is_affiliated=*/true);
  user_manager->SetUserAffiliated(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kSecondUserCorpExampleComEmail,
          kSecondSAMLUserGaiaId),
      /*is_affiliated=*/true);
  user_manager->SetUserAffiliated(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kThirdUserCorpExampleComEmail, kThirdSAMLUserGaiaId),
      /*is_affiliated=*/true);
  user_manager->SetUserAffiliated(
      AccountId::FromUserEmailGaiaId(kNonSAMLUserEmail, kNonSAMLUserGaiaId),
      /*is_affiliated=*/true);

  // Set up fake networks.
  ShillManagerClient::Get()->GetTestInterface()->SetupDefaultEnvironment();
}

void SAMLPolicyTest::SetUpCommandLine(base::CommandLine* command_line) {
  SamlTestBase::SetUpCommandLine(command_line);

  // Disable token check to allow offline sign-in for the fake gaia users.
  command_line->AppendSwitch(switches::kDisableGaiaServices);
}

void SAMLPolicyTest::SetSAMLOfflineSigninTimeLimitPolicy(int limit) {
  policy::PolicyMap user_policy;
  user_policy.Set(policy::key::kSAMLOfflineSigninTimeLimit,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, base::Value(limit), nullptr);
  provider_.UpdateChromePolicy(user_policy);
  base::RunLoop().RunUntilIdle();
}

void SAMLPolicyTest::EnableTransferSAMLCookiesPolicy() {
  base::test::TestFuture<void> test_future;
  base::CallbackListSubscription subscription =
      CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefTransferSAMLCookies, test_future.GetRepeatingCallback());
  std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
      device_state_.RequestDevicePolicyUpdate();
  em::ChromeDeviceSettingsProto& proto(*device_policy_update->policy_payload());
  proto.mutable_saml_settings()->set_transfer_saml_cookies(true);
  device_policy_update.reset();
  ASSERT_TRUE(test_future.Wait());
}

void SAMLPolicyTest::SetLoginBehaviorPolicyToSAMLInterstitial() {
  base::test::TestFuture<void> test_future;
  base::CallbackListSubscription subscription =
      CrosSettings::Get()->AddSettingsObserver(
          kLoginAuthenticationBehavior, test_future.GetRepeatingCallback());

  std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
      device_state_.RequestDevicePolicyUpdate();
  em::ChromeDeviceSettingsProto& proto(*device_policy_update->policy_payload());
  proto.mutable_login_authentication_behavior()
      ->set_login_authentication_behavior(
          em::LoginAuthenticationBehaviorProto_LoginBehavior_SAML_INTERSTITIAL);
  device_policy_update.reset();
  ASSERT_TRUE(test_future.Wait());
}

void SAMLPolicyTest::SetLoginVideoCaptureAllowedUrls(
    const std::vector<GURL>& allowed) {
  base::test::TestFuture<void> test_future;
  base::CallbackListSubscription subscription =
      CrosSettings::Get()->AddSettingsObserver(
          kLoginVideoCaptureAllowedUrls, test_future.GetRepeatingCallback());
  std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
      device_state_.RequestDevicePolicyUpdate();
  em::ChromeDeviceSettingsProto& proto(*device_policy_update->policy_payload());
  for (const GURL& url : allowed)
    proto.mutable_login_video_capture_allowed_urls()->add_urls(url.spec());
  device_policy_update.reset();
  ASSERT_TRUE(test_future.Wait());
}

void SAMLPolicyTest::SetPolicyToHideUserPods() {
  base::test::TestFuture<void> test_future;
  base::CallbackListSubscription subscription =
      CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefShowUserNamesOnSignIn,
          test_future.GetRepeatingCallback());
  std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
      device_state_.RequestDevicePolicyUpdate();
  em::ChromeDeviceSettingsProto& proto(*device_policy_update->policy_payload());
  proto.mutable_show_user_names()->set_show_user_names(false);
  device_policy_update.reset();
  ASSERT_TRUE(test_future.Wait());
}

void SAMLPolicyTest::SetSSOProfile(const std::string& sso_profile) {
  std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
      device_state_.RequestDevicePolicyUpdate();
  device_policy_update->policy_data()->set_sso_profile(sso_profile);
}

void SAMLPolicyTest::ShowGAIALoginForm() {
  LoginDisplayHost::default_host()->StartWizard(GaiaView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  content::DOMMessageQueue message_queue(GetLoginUI()->GetWebContents());
  ASSERT_TRUE(content::ExecJs(
      GetLoginUI()->GetWebContents(),
      "$('gaia-signin').authenticator.addEventListener('ready', function() {"
      "  window.domAutomationController.send('ready');"
      "});"));
  ASSERT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"ready\"");
}

void SAMLPolicyTest::ShowSAMLLoginForm() {
  MaybeWaitForLoginScreenLoad();
  ASSERT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kSigninFrameDialog)->Wait();
  test::OobeJS().CreateVisibilityWaiter(false, kGaiaLoading)->Wait();
}

void SAMLPolicyTest::MaybeWaitForSAMLToLoad() {
  // If SAML already loaded return and otherwise wait for the SamlLoaded
  // message.
  if (test::OobeJS().GetAttributeBool("isSamlAuthFlowForTesting()",
                                      {"gaia-signin"}))
    return;
  content::DOMMessageQueue message_queue(GetLoginUI()->GetWebContents());
  SetupAuthFlowChangeListener();
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"SamlLoaded\"");
  test::OobeJS().ExpectAttributeEQ("isSamlAuthFlowForTesting()",
                                   {"gaia-signin"}, true);
}

void SAMLPolicyTest::ClickBackOnSAMLPage() {
  test::OobeJS().TapOnPath(kSamlBackButton);
}

void SAMLPolicyTest::LogInWithSAML(const std::string& user_id,
                                   const std::string& auth_sid_cookie,
                                   const std::string& auth_lsid_cookie) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(user_id);

  fake_gaia_.fake_gaia()->SetConfigurationHelper(user_id, auth_sid_cookie,
                                                 auth_lsid_cookie);
  fake_gaia_.SetupFakeGaiaForLogin(user_id, "", kTestRefreshToken);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  // Scraping a single password should finish the login right away.
  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();
}

void SAMLPolicyTest::AddSamlUserWithZeroOfflineTimeLimit() {
  SetSAMLOfflineSigninTimeLimitPolicy(0);

  ShowGAIALoginForm();
  // Verify that default Gaia path is used by the Gaia screen when adding a new
  // user.
  EXPECT_EQ(GaiaPath(), WizardContext::GaiaPath::kDefault);
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
}

std::string SAMLPolicyTest::GetCookieValue(const std::string& name) {
  for (net::CookieList::const_iterator it = cookie_list_.begin();
       it != cookie_list_.end(); ++it) {
    if (it->Name() == name)
      return it->Value();
  }
  return std::string();
}

void SAMLPolicyTest::GetCookies(Profile* profile) {
  if (!profile) {
    profile = ProfileManager::GetActiveUserProfile();
    ASSERT_TRUE(profile);
  }
  base::RunLoop run_loop;
  profile->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetAllCookies(base::BindLambdaForTesting(
          [&](const std::vector<net::CanonicalCookie>& cookies) {
            cookie_list_ = cookies;
            run_loop.Quit();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_NoSAML) {
  // Set the offline login time limit for SAML users to zero.
  SetSAMLOfflineSigninTimeLimitPolicy(0);

  WaitForSigninScreen();

  fake_gaia_.fake_gaia()->SetConfigurationHelper(
      kNonSAMLUserEmail, FakeGaiaMixin::kFakeSIDCookie,
      FakeGaiaMixin::kFakeLSIDCookie);
  fake_gaia_.SetupFakeGaiaForLogin(kNonSAMLUserEmail, "", kTestRefreshToken);

  // Log in without SAML.
  LoginDisplayHost::default_host()->StartWizard(GaiaView::kScreenId);
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(kNonSAMLUserEmail, "password", "[]");
  test::WaitForPrimaryUserSessionStart();
  // Fake OAUTH token for this user will be invalidated in-session.
  // Pretend that it's still valid.
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmail(kNonSAMLUserEmail),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
}

// Verifies that the offline login time limit does not affect a user who
// authenticated without SAML.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, NoSAML) {
  const auto account_id = AccountId::FromUserEmail(kNonSAMLUserEmail);
  cryptohome_mixin_.ApplyAuthConfig(
      account_id, test::UserAuthConfig::Create(test::kDefaultAuthSetup));

  // Verify that offline login is allowed.
  LoginScreenTestApi::SubmitPassword(account_id, "password",
                                     true /* check_if_submittable */);
  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SAMLNoLimit) {
  // Remove the offline login time limit for SAML users.
  SetSAMLOfflineSigninTimeLimitPolicy(-1);

  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
}

// Verifies that when no offline login time limit is set, a user who
// authenticated with SAML is allowed to log in offline.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLNoLimit) {
  const auto account_id =
      AccountId::FromUserEmail(saml_test_users::kFirstUserCorpExampleComEmail);
  cryptohome_mixin_.ApplyAuthConfig(
      account_id, test::UserAuthConfig::Create(test::kDefaultAuthSetup));

  // Verify that offline login is allowed.
  LoginScreenTestApi::SubmitPassword(account_id, "password",
                                     true /* check_if_submittable */);
  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SAMLZeroLimit) {
  AddSamlUserWithZeroOfflineTimeLimit();
}

// Verifies that when the offline login time limit is exceeded for a user who
// authenticated via SAML, that user is forced to log in online the next time.
// Also checks Gaia path used by the Gaia screen during online reauth.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLZeroLimit) {
  AccountId account_id =
      AccountId::FromUserEmail(saml_test_users::kFirstUserCorpExampleComEmail);
  // Verify that offline login is not allowed.
  ASSERT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id));

  // Focus triggers online sign-in.
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(account_id));

  // Wait for online authentication screen to show up.
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kSigninFrameDialog)->Wait();
  test::OobeJS().CreateVisibilityWaiter(false, kGaiaLoading)->Wait();

  // Verify that reauth Gaia path is used by the Gaia screen.
  EXPECT_EQ(GaiaPath(), WizardContext::GaiaPath::kReauth);
}

// Add a SAML user who is forced to always authenticate online.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SamlReauthWithSamlRedirect) {
  AddSamlUserWithZeroOfflineTimeLimit();
}

// Checks Gaia path used by the Gaia screen during online reauth with device
// policy set to navigate directly to the SAML login page.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SamlReauthWithSamlRedirect) {
  // Set device policy to navigate directly to SAML page during online sign-in
  SetLoginBehaviorPolicyToSAMLInterstitial();

  AccountId account_id =
      AccountId::FromUserEmail(saml_test_users::kFirstUserCorpExampleComEmail);
  // Verify that offline login is not allowed.
  ASSERT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id));

  // Focus triggers online sign-in.
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(account_id));

  // Wait for online authentication screen to show up.
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kSigninFrameDialog)->Wait();
  test::OobeJS().CreateVisibilityWaiter(false, kGaiaLoading)->Wait();

  // Since this is a reauth of existing user, we expect them to use reauth
  // endpoint regardless of LoginAuthenticationBehavior policy.
  EXPECT_EQ(GaiaPath(), WizardContext::GaiaPath::kReauth);
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_PRE_TransferCookiesAffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue1);
  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Verifies that when the DeviceTransferSAMLCookies policy is not enabled, SAML
// IdP cookies are not transferred to a user's profile on subsequent login, even
// if the user belongs to the domain that the device is enrolled into. Also
// verifies that GAIA cookies are not transferred.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_TransferCookiesAffiliated) {
  cryptohome_mixin_.ApplyAuthConfig(
      AccountId::FromUserEmail(saml_test_users::kFirstUserCorpExampleComEmail),
      test::UserAuthConfig::Create(test::kDefaultAuthSetup));

  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue2);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie2, kTestAuthLSIDCookie2);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Verifies that when the DeviceTransferSAMLCookies policy is enabled, SAML IdP
// cookies are transferred to a user's profile on subsequent login when the user
// belongs to the domain that the device is enrolled into. Also verifies that
// GAIA cookies are not transferred.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, TransferCookiesAffiliated) {
  cryptohome_mixin_.ApplyAuthConfig(
      AccountId::FromUserEmail(saml_test_users::kFirstUserCorpExampleComEmail),
      test::UserAuthConfig::Create(test::kDefaultAuthSetup));

  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue2);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  ShowGAIALoginForm();

  EnableTransferSAMLCookiesPolicy();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie2, kTestAuthLSIDCookie2);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue2, GetCookieValue(kSAMLIdPCookieName));
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_TransferCookiesUnaffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue1);
  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFifthUserExampleTestEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Verifies that even if the DeviceTransferSAMLCookies policy is enabled, SAML
// IdP are not transferred to a user's profile on subsequent login if the user
// does not belong to the domain that the device is enrolled into. Also verifies
// that GAIA cookies are not transferred.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, TransferCookiesUnaffiliated) {
  cryptohome_mixin_.ApplyAuthConfig(
      AccountId::FromUserEmail(saml_test_users::kFifthUserExampleTestEmail),
      test::UserAuthConfig::Create(test::kDefaultAuthSetup));

  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue2);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  ShowGAIALoginForm();

  EnableTransferSAMLCookiesPolicy();
  LogInWithSAML(saml_test_users::kFifthUserExampleTestEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Tests that the SAML page is loaded when the authentication behavior device
// policy is set to SAML_INTERSTITIAL, and when the user clicks the "Enter
// Google Account info" button, we go to the default GAIA signin screen.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLChangeAccount) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  SetLoginBehaviorPolicyToSAMLInterstitial();
  WaitForSigninScreen();

  ShowSAMLLoginForm();

  // Verify Gaia path used by the Gaia screen.
  EXPECT_EQ(GaiaPath(), WizardContext::GaiaPath::kSamlRedirect);

  // Adding a cookie to the signin profile.
  // After the "Enter Google Account info" button is pressed, all the
  // cookies must be deleted.
  net::CookieOptions options;
  options.set_include_httponly();
  auto* profile = ProfileHelper::Get()->GetSigninProfile();
  ASSERT_TRUE(profile);
  profile->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetCanonicalCookie(
          *net::CanonicalCookie::CreateSanitizedCookie(
              fake_saml_idp()->GetSamlPageUrl(), kSAMLIdPCookieName,
              kSAMLIdPCookieValue1, ".example.com", /*path=*/std::string(),
              /*creation_time=*/base::Time(),
              /*expiration_time=*/base::Time(),
              /*last_access_time=*/base::Time(), /*secure=*/true,
              /*http_only=*/false, net::CookieSameSite::NO_RESTRICTION,
              net::COOKIE_PRIORITY_DEFAULT,
              /*partition_key=*/std::nullopt, /*status=*/nullptr),
          fake_saml_idp()->GetSamlPageUrl(), options, base::DoNothing());

  // Click the "Enter Google Account info" button on the SAML page.
  test::OobeJS().TapOnPath(kChangeIdPButton);

  // Expects that the gaia signin frame is shown.
  test::OobeJS().CreateVisibilityWaiter(true, kSigninFrameDialog)->Wait();
  test::OobeJS().CreateVisibilityWaiter(false, kGaiaLoading)->Wait();
  test::OobeJS().ExpectHasNoAttribute("transparent", kSigninFrameDialog);
  test::OobeJS().ExpectAttributeEQ("isSamlAuthFlowForTesting()",
                                   {"gaia-signin"}, false);

  EXPECT_EQ(GaiaPath(), WizardContext::GaiaPath::kDefault);

  GetCookies(profile);
  EXPECT_EQ("", GetCookieValue(kSAMLIdPCookieName));
}

// Tests that clicking back on the SAML page successfully closes the oobe
// dialog. Reopens a dialog and checks that SAML IdP authentication page is
// loaded and authenticating there is successful.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLClosaAndReopen) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  fake_gaia_.fake_gaia()->SetConfigurationHelper(
      saml_test_users::kFirstUserCorpExampleComEmail, kTestAuthSIDCookie1,
      kTestAuthLSIDCookie1);
  SetLoginBehaviorPolicyToSAMLInterstitial();
  WaitForSigninScreen();

  ShowSAMLLoginForm();
  ClickBackOnSAMLPage();

  // Back button should hide OOBE dialog.
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(LoginScreenTestApi::IsAddUserButtonShown());

  ShowSAMLLoginForm();
  MaybeWaitForSAMLToLoad();

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  // Scraping one password should finish login.
  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SamlToGaiaChange) {
  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  user_manager::KnownUser known_user(g_browser_process->local_state());
  EXPECT_TRUE(known_user.IsUsingSAML(AccountId::FromUserEmailGaiaId(
      saml_test_users::kFirstUserCorpExampleComEmail, kFirstSAMLUserGaiaId)));
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  ASSERT_TRUE(user);
  EXPECT_TRUE(user->using_saml());
}

// Verifies that when SAML user changes IdP to GAIA we detect it correctly and
// update corresponding flags in the local state and in user session.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SamlToGaiaChange) {
  const auto account_id = AccountId::FromUserEmailGaiaId(
      saml_test_users::kFirstUserCorpExampleComEmail, kFirstSAMLUserGaiaId);
  cryptohome_mixin_.ApplyAuthConfig(
      account_id, test::UserAuthConfig::Create(test::kDefaultAuthSetup));

  // Change user IdP to GAIA.
  fake_gaia_.fake_gaia()->RemoveSamlIdpForUser(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Log in without SAML.
  LoginDisplayHost::default_host()->StartWizard(GaiaView::kScreenId);
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(saml_test_users::kFirstUserCorpExampleComEmail,
                                "password", "[]");
  test::WaitForPrimaryUserSessionStart();

  user_manager::KnownUser known_user(g_browser_process->local_state());
  EXPECT_FALSE(known_user.IsUsingSAML(account_id));
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  ASSERT_TRUE(user);
  EXPECT_FALSE(user->using_saml());
}

// Ensure that the permission status of getUserMedia requests from SAML login
// pages is controlled by the kLoginVideoCaptureAllowedUrls pref rather than the
// underlying user content setting.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, TestLoginMediaPermission) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  SetLoginBehaviorPolicyToSAMLInterstitial();
  WaitForSigninScreen();

  ShowSAMLLoginForm();
  MaybeWaitForSAMLToLoad();

  const GURL url0(fake_saml_idp()->GetSamlPageUrl());
  const GURL url1("https://google.com");
  const GURL url2("https://corp.example.com");
  const GURL url3("https://not-allowed.com");
  SetLoginVideoCaptureAllowedUrls({url0, url1, url2});

  // Trigger the permission check from the js side.
  ASSERT_EQ(true, content::EvalJs(SigninFrameJS().web_contents(),
                                  "new Promise(resolve => {"
                                  "  navigator.getUserMedia("
                                  "    {video: true},"
                                  "    function() { resolve(true); },"
                                  "    function() { resolve(false); });"
                                  "});"));
  ASSERT_EQ(false, content::EvalJs(SigninFrameJS().web_contents(),
                                   "new Promise(resolve => {"
                                   "  navigator.getUserMedia("
                                   "    {audio: true},"
                                   "    function() { resolve(true); },"
                                   "    function() { resolve(false); });"
                                   "});"));

  // Check permissions directly by calling the `CheckMediaAccessPermission`.
  content::WebContents* web_contents = GetLoginUI()->GetWebContents();
  content::WebContentsDelegate* web_contents_delegate =
      web_contents->GetDelegate();

  // Mic should always be blocked.
  EXPECT_FALSE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(url1),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));

  // Camera should be allowed if allowed by the allowlist, otherwise blocked.
  EXPECT_TRUE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(url1),
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  EXPECT_TRUE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(url2),
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  EXPECT_FALSE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(url3),
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  // Camera should be blocked in the login screen, even if it's allowed via
  // content setting.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(url3, url3,
                                      ContentSettingsType::MEDIASTREAM_CAMERA,
                                      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(url3),
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}

// Tests that requesting webcam access from the lock screen works correctly.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, TestLockMediaPermission) {
  SetSAMLOfflineSigninTimeLimitPolicy(0);
  const GURL url0(fake_saml_idp()->GetSamlPageUrl());
  const GURL url1("https://google.com");
  const GURL url2("https://corp.example.com");
  const GURL url3("https://not-allowed.com");
  SetLoginVideoCaptureAllowedUrls({url0, url1, url2});
  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();
  ASSERT_TRUE(reauth_dialog_helper);

  ASSERT_EQ(true, content::EvalJs(
                      reauth_dialog_helper->SigninFrameJS().web_contents(),
                      "new Promise(resolve => {"
                      "  navigator.getUserMedia("
                      "    {video: true},"
                      "    function() { resolve(true); },"
                      "    function() { resolve(false); });"
                      "});"));
  ASSERT_EQ(false, content::EvalJs(
                       reauth_dialog_helper->SigninFrameJS().web_contents(),
                       "new Promise(resolve => {"
                       "  navigator.getUserMedia("
                       "    {audio: true},"
                       "    function() { resolve(true); },"
                       "    function() { resolve(false); });"
                       "});"));

  // Check permissions directly by calling the `CheckMediaAccessPermission`.
  // Video devices should be allowed from the lock screen for specified urls.
  content::WebContents* web_contents =
      reauth_dialog_helper->DialogWebContents();
  content::WebContentsDelegate* web_contents_delegate =
      web_contents->GetDelegate();

  // Mic should always be blocked.
  EXPECT_FALSE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(url1),
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));

  // Camera should be allowed if allowed by the allowlist, otherwise blocked.
  EXPECT_TRUE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(url1),
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  EXPECT_TRUE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(url2),
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  EXPECT_FALSE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetPrimaryMainFrame(), url::Origin::Create(url3),
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}

// Tests that we land on 3P IdP page corresponding to sso_profile from the
// device policy blob during online auth with hidden user pods.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SsoProfileInHiddenUserPodsFlow) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  // Set wrong redirect url for domain-based SAML redirection. This ensures that
  // for test to finish successfully it should perform redirection based on SSO
  // profile.
  const GURL wrong_redirect_url("https://wrong.com");
  fake_gaia_.fake_gaia()->RegisterSamlDomainRedirectUrl(
      fake_saml_idp()->GetIdpDomain(), wrong_redirect_url);

  SetLoginBehaviorPolicyToSAMLInterstitial();
  SetSSOProfile(fake_saml_idp()->GetIdpSsoProfile());
  SetPolicyToHideUserPods();

  // Wait for online authentication screen to show up.
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kSigninFrameDialog)->Wait();
  test::OobeJS().CreateVisibilityWaiter(false, kGaiaLoading)->Wait();

  // Expect that we are in the SAML flow.
  test::OobeJS().ExpectVisiblePath(kSamlNoticeContainer);
  // Verify Gaia path used by the Gaia screen.
  EXPECT_EQ(GaiaPath(), WizardContext::GaiaPath::kSamlRedirect);
  // Expect email field to be visible on IdP page - this guarantees that we've
  // navigated to the IdP page based on SSO profile since we've set domain-based
  // redirection to not work above.
  SigninFrameJS().ExpectVisible("Email");
}

// Tests that we land on 3P IdP page corresponding to sso_profile from the
// device policy blob during "add user" flow.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SsoProfileInAddNewUserFlow) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  // Set wrong redirect url for domain-based SAML redirection. This ensures that
  // for test to finish successfully it should perform redirection based on SSO
  // profile.
  const GURL wrong_redirect_url("https://wrong.com");
  fake_gaia_.fake_gaia()->RegisterSamlDomainRedirectUrl(
      fake_saml_idp()->GetIdpDomain(), wrong_redirect_url);

  SetLoginBehaviorPolicyToSAMLInterstitial();
  SetSSOProfile(fake_saml_idp()->GetIdpSsoProfile());

  // Launch "add user" flow.
  ShowSAMLLoginForm();

  // Expect that we are in the SAML flow.
  test::OobeJS().ExpectVisiblePath(kSamlNoticeContainer);
  // Verify Gaia path used by the Gaia screen.
  EXPECT_EQ(GaiaPath(), WizardContext::GaiaPath::kSamlRedirect);
  // Expect email field to be visible on IdP page - this guarantees that we've
  // navigated to the IdP page based on SSO profile since we've set domain-based
  // redirection to not work above.
  SigninFrameJS().ExpectVisible("Email");
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLBlocklistNavigationDisallowed) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_link.html");
  SetLoginBehaviorPolicyToSAMLInterstitial();

  // TODO(https://issuetracker.google.com/290830337): Make this test class
  // support propagating device policies to prefs with the logic in
  // `LoginProfilePolicyProvider`, and instead of setting prefs here directly,
  // just set the right device policies using
  // `DeviceStateMixin::RequestDevicePolicyUpdate`.
  // TODO(https://issuetracker.google.com/290821299): Add browser tests for
  // allowlisting.
  Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetSigninBrowserContext())
      ->GetPrefs()
      ->SetList(policy::policy_prefs::kUrlBlocklist,
                base::Value::List().Append(kSAMLLinkedPageURLPattern));

  ShowSAMLLoginForm();

  test::TestPredicateWaiter(base::BindRepeating([]() {
    return LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetHandler<GaiaScreenHandler>()
        ->IsLoadedForTesting();
  })).Wait();

  SigninFrameJS().CreateVisibilityWaiter(true, kSAMLLink)->Wait();
  SigninFrameJS().TapOn(kSAMLLink);
  WaitForLoadStop(SigninFrameJS().web_contents());

  SigninFrameJS()
      .CreateElementTextContentWaiter(
          l10n_util::GetStringUTF8(
              IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR),
          kErrorPage)
      ->Wait();

  test::TestPredicateWaiter(base::BindRepeating([]() {
    return LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetHandler<GaiaScreenHandler>()
        ->IsNavigationBlockedForTesting();
  })).Wait();
}

// Pushes DeviceLoginScreenLocales into the device policy.
class SAMLLocalesTest : public SamlTestBase {
 public:
  constexpr static char kLoginLocale[] = "de";
  SAMLLocalesTest() {
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
    auto policy_update = device_state_.RequestDevicePolicyUpdate();
    em::ChromeDeviceSettingsProto& proto(*policy_update->policy_payload());
    *proto.mutable_login_screen_locales()->add_login_screen_locales() =
        kLoginLocale;
  }
};

// Tests that DeviceLoginLocales policy propagates to the 3rd-party IdP. We use
// a separate `SAMLLocalesTest` test fixture for this instead of expanding
// `SAMLPolicyTest` because dynamically reloading the language on policy change
// is hard.
IN_PROC_BROWSER_TEST_F(SAMLLocalesTest, PropagatesToIdp) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().ExpectEQ("navigator.language", std::string(kLoginLocale));
}

class SAMLPasswordAttributesTest : public SAMLPolicyTest,
                                   public testing::WithParamInterface<bool> {
 public:
  SAMLPasswordAttributesTest() = default;

  SAMLPasswordAttributesTest(const SAMLPasswordAttributesTest&) = delete;
  SAMLPasswordAttributesTest& operator=(const SAMLPasswordAttributesTest&) =
      delete;

  void SetUpOnMainThread() override;

 protected:
  bool in_session_pw_change_policy_enabled() { return GetParam(); }
};

void SAMLPasswordAttributesTest::SetUpOnMainThread() {
  policy::PolicyMap user_policy;
  user_policy.Set(policy::key::kSamlInSessionPasswordChangeEnabled,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD,
                  base::Value(in_session_pw_change_policy_enabled()), nullptr);
  provider_.UpdateChromePolicy(user_policy);
  base::RunLoop().RunUntilIdle();

  SAMLPolicyTest::SetUpOnMainThread();
}

// Verifies that password attributes are extracted and stored during a
// successful log in - but only if the appropriate policy is enabled.
IN_PROC_BROWSER_TEST_P(SAMLPasswordAttributesTest, LoginSucceeded) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  fake_saml_idp()->SetSamlResponseFile("saml_with_password_attributes.xml");
  ShowGAIALoginForm();
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();

  Profile* profile = ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetPrimaryUser());
  SamlPasswordAttributes attrs =
      SamlPasswordAttributes::LoadFromPrefs(profile->GetPrefs());

  if (in_session_pw_change_policy_enabled()) {
    // These values are extracted from saml_with_password_attributes.xml
    EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(1550836258421L),
              attrs.modified_time());
    EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(1551873058421L),
              attrs.expiration_time());
    EXPECT_EQ("https://" + fake_saml_idp()->GetIdpDomain() +
                  "/adfs/portal/updatepassword/",
              attrs.password_change_url());
  } else {
    // Nothing should be extracted when policy is disabled.
    EXPECT_FALSE(attrs.has_modified_time());
    EXPECT_FALSE(attrs.has_expiration_time());
    EXPECT_FALSE(attrs.has_password_change_url());
  }
}

// Verify that no password attributes are stored when login fails.
// TODO(crbug.com/325657256): Test is flaky.
IN_PROC_BROWSER_TEST_P(SAMLPasswordAttributesTest, DISABLED_LoginFailed) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  fake_saml_idp()->SetSamlResponseFile("saml_with_password_attributes.xml");
  ShowGAIALoginForm();
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Give fake gaia an empty email address, so login will fail:
  fake_gaia_.fake_gaia()->SetConfigurationHelper(
      /*email=*/"", kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  // SAML login fails:
  ExpectFatalErrorMessage(
      l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS));

  test::OobeJS().TapOnPath(kFatalErrorActionButton);
  WaitForSigninScreen();

  // Make sure no SAML password attributes are saved.
  // None are saved for the logged in user, since there is no logged in user:
  EXPECT_EQ(nullptr, user_manager::UserManager::Get()->GetPrimaryUser());

  // Also, no attributes are saved in the signin profile:
  Profile* profile = ProfileHelper::Get()->GetSigninProfile();
  SamlPasswordAttributes attrs =
      SamlPasswordAttributes::LoadFromPrefs(profile->GetPrefs());
  EXPECT_FALSE(attrs.has_modified_time());
  EXPECT_FALSE(attrs.has_expiration_time());
  EXPECT_FALSE(attrs.has_password_change_url());
}

INSTANTIATE_TEST_SUITE_P(All, SAMLPasswordAttributesTest, testing::Bool());

void FakeGetCertificateCallbackTrue(
    attestation::AttestationFlow::CertificateCallback callback) {
  // In reality, attestation service holds the certificate after a successful
  // attestation flow.
  AttestationClient::Get()
      ->GetTestInterface()
      ->GetMutableKeyInfoReply(/*username=*/"",
                               attestation::kEnterpriseMachineKey)
      ->set_certificate("certificate");
  std::move(callback).Run(attestation::ATTESTATION_SUCCESS, "certificate");
}

constexpr base::TimeDelta kTimeoutTaskDelay = base::Milliseconds(500);
constexpr base::TimeDelta kBuildResponseTaskDelay = base::Seconds(3);
static_assert(
    kTimeoutTaskDelay < kBuildResponseTaskDelay,
    "kTimeoutTaskDelay should be less than kBuildResponseTaskDelay to trigger "
    "timeout error in SAMLDeviceAttestationTest.TimeoutError test.");

class SAMLDeviceAttestationTest : public SamlTestBase {
 public:
  SAMLDeviceAttestationTest() = default;

  SAMLDeviceAttestationTest(const SAMLDeviceAttestationTest&) = delete;
  SAMLDeviceAttestationTest& operator=(const SAMLDeviceAttestationTest&) =
      delete;

  void SetUpInProcessBrowserTestFixture() override;

 protected:
  virtual void SetAllowedUrlsPolicy(
      const std::vector<std::string>& allowed_urls);
  void SetDeviceContextAwareAccessSignalsAllowlistPolicy(
      const std::vector<std::string>& allowed_urls);

  ScopedTestingCrosSettings settings_helper_;
  raw_ptr<StubCrosSettingsProvider> settings_provider_ = nullptr;

  policy::DevicePolicyCrosTestHelper policy_helper_;

  attestation::MockMachineCertificateUploader mock_cert_uploader_;
  NiceMock<attestation::MockAttestationFlow> mock_attestation_flow_;
  ScopedStubInstallAttributes stub_install_attributes_;
};

void SAMLDeviceAttestationTest::SetUpInProcessBrowserTestFixture() {
  SamlTestBase::SetUpInProcessBrowserTestFixture();

  settings_provider_ = settings_helper_.device_settings();

  ON_CALL(mock_attestation_flow_, GetCertificate)
      .WillByDefault(WithArgs<7>(Invoke(FakeGetCertificateCallbackTrue)));

  // By default make it reply that the certificate is already uploaded.
  ON_CALL(mock_cert_uploader_, WaitForUploadComplete)
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<0>(
          /*certificate_uploaded=*/true));

  attestation::TpmChallengeKeyFactory::SetForTesting(
      std::make_unique<attestation::TpmChallengeKeyImpl>(
          &mock_attestation_flow_, &mock_cert_uploader_));

  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
}

void SAMLDeviceAttestationTest::SetAllowedUrlsPolicy(
    const std::vector<std::string>& allowed_urls) {
  base::Value::List allowed_urls_values;
  for (const auto& url : allowed_urls) {
    allowed_urls_values.Append(url);
  }
  settings_provider_->Set(kDeviceWebBasedAttestationAllowedUrls,
                          base::Value(std::move(allowed_urls_values)));
}

void SAMLDeviceAttestationTest::
    SetDeviceContextAwareAccessSignalsAllowlistPolicy(
        const std::vector<std::string>& allowed_urls) {
  em::StringListPolicyProto* allowed_urls_proto =
      policy_helper_.device_policy()
          ->payload()
          .mutable_device_login_screen_context_aware_access_signals_allowlist();
  allowed_urls_proto->mutable_policy_options()->set_mode(
      em::PolicyOptions::MANDATORY);
  for (const auto& url : allowed_urls) {
    allowed_urls_proto->mutable_value()->add_entries(url.c_str());
  }
  policy_helper_.RefreshDevicePolicy();
}

class SAMLDeviceAttestationEnrolledTest : public SAMLDeviceAttestationTest {
 public:
  SAMLDeviceAttestationEnrolledTest() {
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SAMLDeviceAttestationTest::SetUpInProcessBrowserTestFixture();
    stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  }

  base::HistogramTester histogram_tester_;
};

// Verify that device attestation is not available when
// DeviceWebBasedAttestationAllowedUrls policy is not set.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, DefaultPolicy) {
  // Leave policy unset.

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
}

// Verify that device attestation is not available when
// DeviceWebBasedAttestationAllowedUrls policy is set to empty list of allowed
// URLs.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, EmptyPolicy) {
  SetAllowedUrlsPolicy({/* empty list */});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
}

// Verify that device attestation is not available when device is not enterprise
// enrolled.
// TODO(crbug.com/1407565): Re-enable when no longer flaky.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_NotEnterpriseEnrolledError DISABLED_NotEnterpriseEnrolledError
#else
#define MAYBE_NotEnterpriseEnrolledError NotEnterpriseEnrolledError
#endif
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest,
                       MAYBE_NotEnterpriseEnrolledError) {
  SetAllowedUrlsPolicy({fake_saml_idp()->GetIdpHost()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
}

// Verify that device attestation works when all policies configured correctly.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationEnrolledTest, Success) {
  SetAllowedUrlsPolicy({fake_saml_idp()->GetIdpHost()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  histogram_tester_.ExpectBucketCount(kDeviceTrustMatchHistogramName, false, 1);

  ASSERT_TRUE(fake_saml_idp()->IsLastChallengeResponseExists());
  ASSERT_NO_FATAL_FAILURE(
      fake_saml_idp()->AssertChallengeResponseMatchesTpmResponse());
}

// Verify that device attestation is not available for URLs that are not in the
// allowed URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationEnrolledTest, PolicyNoMatchError) {
  SetAllowedUrlsPolicy({fake_saml_idp()->GetIdpDomain()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  histogram_tester_.ExpectTotalCount(kDeviceTrustMatchHistogramName, 0);

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
}

// Verify that device attestation is available for URLs that match a pattern
// from allowed URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationEnrolledTest, PolicyRegexSuccess) {
  SetAllowedUrlsPolicy({"[*.]" + fake_saml_idp()->GetIdpDomain()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  histogram_tester_.ExpectBucketCount(kDeviceTrustMatchHistogramName, false, 1);

  ASSERT_TRUE(fake_saml_idp()->IsLastChallengeResponseExists());
  ASSERT_NO_FATAL_FAILURE(
      fake_saml_idp()->AssertChallengeResponseMatchesTpmResponse());
}

// Verify that device attestation works in case of multiple items in allowed
// URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationEnrolledTest,
                       PolicyTwoEntriesSuccess) {
  SetAllowedUrlsPolicy({"example2.com", fake_saml_idp()->GetIdpHost()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  histogram_tester_.ExpectBucketCount(kDeviceTrustMatchHistogramName, false, 1);

  ASSERT_TRUE(fake_saml_idp()->IsLastChallengeResponseExists());
  ASSERT_NO_FATAL_FAILURE(
      fake_saml_idp()->AssertChallengeResponseMatchesTpmResponse());
}

// Verify that device attestation is not available for URLs that also match the
// ones on the DeviceContextAwareAccessSignalsAllowlist. Instead, device trust
// is doing the attestation handshake.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationEnrolledTest,
                       PolicyDeviceTrustMatchError) {
  SetAllowedUrlsPolicy({fake_saml_idp()->GetIdpHost()});
  SetDeviceContextAwareAccessSignalsAllowlistPolicy(
      {fake_saml_idp()->GetIdpHost()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  // TODO(b/249082222) kDeviceTrustMatchHistogramName should get called once
  // with value "true"
  histogram_tester_.ExpectTotalCount(kDeviceTrustMatchHistogramName, 0);

  ASSERT_EQ(fake_saml_idp()->GetChallengeResponseCount(), 1);

  // TODO(b:253427534): Handle VA V1 challenges cases for Device Trust.
  histogram_tester_.ExpectBucketCount(
      kDeviceTrustAttestationFunnelStep,
      enterprise_connectors::DTAttestationFunnelStep::kChallengeResponseSent,
      0);
}

IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationEnrolledTest, TimeoutError) {
  SetAllowedUrlsPolicy({"example2.com", fake_saml_idp()->GetIdpHost()});

  AttestationClient::Get()
      ->GetTestInterface()
      ->set_sign_enterprise_challenge_delay(kBuildResponseTaskDelay);

  auto handler = std::make_unique<SamlChallengeKeyHandler>();
  handler->SetTpmResponseTimeoutForTesting(kTimeoutTaskDelay);

  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetHandler<GaiaScreenHandler>()
      ->SetNextSamlChallengeKeyHandlerForTesting(std::move(handler));

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  histogram_tester_.ExpectBucketCount(kDeviceTrustMatchHistogramName, false, 1);

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
}

// Tests for various flows of Device Trust.
class SAMLDeviceTrustTest : public SAMLDeviceAttestationTest {
 public:
  SAMLDeviceTrustTest() {
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

  void ExpectDeviceTrustSuccessful() {
    ASSERT_TRUE(fake_saml_idp()->DeviceTrustHeaderRecieved());
    ASSERT_TRUE(fake_saml_idp()->IsLastChallengeResponseExists());

    histogram_tester_.ExpectBucketCount(
        kDeviceTrustAttestationFunnelStep,
        enterprise_connectors::DTAttestationFunnelStep::kChallengeResponseSent,
        1);
  }

 private:
  base::HistogramTester histogram_tester_;
};

class SAMLDeviceTrustEnrolledTest : public SAMLDeviceTrustTest {
  void SetUpInProcessBrowserTestFixture() override {
    SAMLDeviceTrustTest::SetUpInProcessBrowserTestFixture();
    stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  }
};

// Verify that device trust is not available when device is not enterprise
// enrolled.
IN_PROC_BROWSER_TEST_F(SAMLDeviceTrustTest, NotEnterpriseEnrolledError) {
  SetDeviceContextAwareAccessSignalsAllowlistPolicy(
      {fake_saml_idp()->GetIdpHost()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSixthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_FALSE(fake_saml_idp()->DeviceTrustHeaderRecieved());
}

// Verify that device trust is not available when
// DeviceLoginScreenContextAwareAccessSignalsAllowlistPolicy policy is not set.
IN_PROC_BROWSER_TEST_F(SAMLDeviceTrustEnrolledTest, DefaultPolicy) {
  // Leave policy unset.

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSixthUserCorpExampleTestEmail);

  ASSERT_FALSE(fake_saml_idp()->DeviceTrustHeaderRecieved());
}

// Verify that device trust is not available when
// DeviceLoginScreenContextAwareAccessSignalsAllowlistPolicy policy is set to
// empty list of allowed URLs.
IN_PROC_BROWSER_TEST_F(SAMLDeviceTrustEnrolledTest, EmptyPolicy) {
  SetDeviceContextAwareAccessSignalsAllowlistPolicy({/* empty list */});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSixthUserCorpExampleTestEmail);

  ASSERT_FALSE(fake_saml_idp()->DeviceTrustHeaderRecieved());
}

// Verify that device trust is available for URLs that match a pattern
// from allowed URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceTrustEnrolledTest, PolicyRegexSuccess) {
  SetDeviceContextAwareAccessSignalsAllowlistPolicy(
      {fake_saml_idp()->GetIdpDomain()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSixthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ExpectDeviceTrustSuccessful();
}

// Verify that Device trust sends the header for a matching URL.
IN_PROC_BROWSER_TEST_F(SAMLDeviceTrustEnrolledTest, SendHeaderForMatchingURL) {
  SetDeviceContextAwareAccessSignalsAllowlistPolicy(
      {fake_saml_idp()->GetIdpHost()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSixthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_TRUE(fake_saml_idp()->DeviceTrustHeaderRecieved());
}

// Verify that Device trust doesn't sends the header for a non matching URL.
IN_PROC_BROWSER_TEST_F(SAMLDeviceTrustEnrolledTest,
                       HeaderNotSentForNonMatchingURL) {
  SetDeviceContextAwareAccessSignalsAllowlistPolicy({"example2.com"});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSixthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_FALSE(fake_saml_idp()->DeviceTrustHeaderRecieved());
}

// Verify that Device trust sends the challenge-response for a matching URL.
IN_PROC_BROWSER_TEST_F(SAMLDeviceTrustEnrolledTest, Success) {
  SetDeviceContextAwareAccessSignalsAllowlistPolicy(
      {fake_saml_idp()->GetIdpHost()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSixthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ExpectDeviceTrustSuccessful();
}

// Verify that device trust works in case of multiple items in allowed
// URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceTrustEnrolledTest, PolicyTwoEntriesSuccess) {
  SetDeviceContextAwareAccessSignalsAllowlistPolicy(
      {fake_saml_idp()->GetIdpHost()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSixthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ExpectDeviceTrustSuccessful();
}

INSTANTIATE_TEST_SUITE_P(All, SamlTestWithFeatures, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         SamlWithCreateAccountAPITest,
                         testing::Combine(testing::Bool(), testing::Bool()));
}  // namespace ash
