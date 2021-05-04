// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/mock_machine_certificate_uploader.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/saml/fake_saml_idp_mixin.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/https_forwarder.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_handler_test_utils.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/saml_challenge_key_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/attestation/fake_attestation_client.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace em = enterprise_management;

using base::test::RunOnceCallback;

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::WithArgs;

namespace chromeos {

namespace {

const test::UIPath kPasswordInput = {"saml-confirm-password", "passwordInput"};
const test::UIPath kPasswordConfirmInput = {"saml-confirm-password",
                                            "confirmPasswordInput"};
const test::UIPath kPasswordSubmit = {"saml-confirm-password", "next"};
const test::UIPath kSamlNoticeMessage = {"gaia-signin", "signin-frame-dialog",
                                         "saml-notice-message"};
const test::UIPath kSamlNoticeContainer = {"gaia-signin", "signin-frame-dialog",
                                           "saml-notice-container"};
constexpr test::UIPath kBackButton = {"gaia-signin", "signin-frame-dialog",
                                      "signin-back-button"};
constexpr test::UIPath kEnterprisePrimaryButton = {
    "enterprise-enrollment", "step-signin", "primary-action-button"};
constexpr test::UIPath kSamlCloseButton = {"gaia-signin", "signin-frame-dialog",
                                           "saml-close-button"};

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

// A FakeUserDataAuthClient that stores the salted and hashed secret passed to
// MountEx().
class SecretInterceptingFakeUserDataAuthClient : public FakeUserDataAuthClient {
 public:
  SecretInterceptingFakeUserDataAuthClient();

  void Mount(const ::user_data_auth::MountRequest& request,
             MountCallback callback) override;

  const std::string& salted_hashed_secret() { return salted_hashed_secret_; }

 private:
  std::string salted_hashed_secret_;

  DISALLOW_COPY_AND_ASSIGN(SecretInterceptingFakeUserDataAuthClient);
};

SecretInterceptingFakeUserDataAuthClient::
    SecretInterceptingFakeUserDataAuthClient() {}

void SecretInterceptingFakeUserDataAuthClient::Mount(
    const ::user_data_auth::MountRequest& request,
    MountCallback callback) {
  salted_hashed_secret_ = request.authorization().key().secret();
  FakeUserDataAuthClient::Mount(request, std::move(callback));
}

}  // namespace

class SamlTest : public OobeBaseTest {
 public:
  SamlTest() {
    // TODO(crbug.com/1121910): Fix tests.
    feature_list_.InitAndDisableFeature(
        chromeos::features::kChildSpecificSignin);
    fake_gaia_.set_initialize_fake_merge_session(false);
  }
  ~SamlTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    command_line->AppendSwitch(
        chromeos::switches::kAllowFailedPolicyFetchForTest);

    // TODO(crbug.com/1177416) - Fix this with a proper SSL solution.
    command_line->AppendSwitch(::switches::kIgnoreCertificateErrors);
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Creates a fake UserDataAuthClient. Will be destroyed in browser shutdown.
    cryptohome_client_ = new SecretInterceptingFakeUserDataAuthClient();

    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    OobeBaseTest::TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    // Allowlist the default EMK to sign enterprise challenge.
    ::attestation::SignEnterpriseChallengeRequest
        sign_enterprise_challenge_request;
    sign_enterprise_challenge_request.set_username("");
    sign_enterprise_challenge_request.set_key_label(
        attestation::kEnterpriseMachineKey);
    sign_enterprise_challenge_request.set_device_id("device_id");
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

    fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
        saml_test_users::kFirstUserCorpExampleComEmail, kTestAuthSIDCookie1,
        kTestAuthLSIDCookie1);

    OobeBaseTest::SetUpOnMainThread();
  }

  void SetupAuthFlowChangeListener() {
    content::ExecuteScriptAsync(
        GetLoginUI()->GetWebContents(),
        "$('gaia-signin').authenticator_.addEventListener('authFlowChange',"
        "    function f() {"
        "      $('gaia-signin').authenticator_.removeEventListener("
        "          'authFlowChange', f);"
        "      window.domAutomationController.send("
        "          $('gaia-signin').isSamlForTesting() ?"
        "              'SamlLoaded' : 'GaiaLoaded');"
        "    });");
  }

  virtual void StartSamlAndWaitForIdpPageLoad(const std::string& gaia_email) {
    OobeScreenWaiter(GetFirstSigninScreen()).Wait();

    content::DOMMessageQueue message_queue;  // Start observe before SAML.
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

    EXPECT_TRUE(ash::LoginScreenTestApi::IsShutdownButtonShown());
    EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
    EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

    test::OobeJS().ExpectElementText(error_message,
                                     {"signin-fatal-error", "subtitle"});
  }

  FakeSamlIdpMixin* fake_saml_idp() { return &fake_saml_idp_mixin_; }

 protected:
  SecretInterceptingFakeUserDataAuthClient* cryptohome_client_;

  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  chromeos::DeviceStateMixin device_state_{
      &mixin_host_, chromeos::DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

 private:
  FakeSamlIdpMixin fake_saml_idp_mixin_{&mixin_host_, &fake_gaia_};

  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SamlTest);
};

// Tests that signin frame should display the SAML notice and the 'back' button
// when SAML IdP page is loaded. And the 'back' button goes back to gaia on
// clicking.
IN_PROC_BROWSER_TEST_F(SamlTest, SamlUI) {
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

  content::DOMMessageQueue message_queue;  // Observe before 'close'.
  SetupAuthFlowChangeListener();
  // Click on 'close'.
  test::OobeJS().ClickOnPath(kSamlCloseButton);

  // Auth flow should change back to Gaia.
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"GaiaLoaded\"");

  // Saml flow is gone.
  test::OobeJS().ExpectHiddenPath(kSamlNoticeContainer);
}

// The SAML IdP requires HTTP Protocol-level authentication (Basic in this
// case).
IN_PROC_BROWSER_TEST_F(SamlTest, IdpRequiresHttpAuth) {
  fake_saml_idp()->SetRequireHttpBasicAuth(true);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");

  // This is not calling StartSamlAndWaitForIdpPageLoad because it has
  // to wait for the auth credentials entry dialog in between. Also, only load
  // the gaia page first so we can get a pointer to the gaia frame's
  // WebContents.
  WaitForGaiaPageLoad();

  content::WebContents* gaia_frame_web_contents =
      signin::GetAuthFrameWebContents(GetLoginUI()->GetWebContents(),
                                      gaia_frame_parent_);
  content::NavigationController* gaia_frame_navigation_controller =
      &(gaia_frame_web_contents->GetController());

  // Start observing before initiating SAML sign-in.
  content::DOMMessageQueue message_queue;
  LoginPromptBrowserTestObserver login_prompt_observer;
  login_prompt_observer.Register(content::Source<content::NavigationController>(
      gaia_frame_navigation_controller));
  WindowedAuthNeededObserver auth_needed_waiter(
      gaia_frame_navigation_controller);

  SetupAuthFlowChangeListener();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(saml_test_users::kFirstUserCorpExampleComEmail,
                                "", "[]");

  auth_needed_waiter.Wait();
  ASSERT_FALSE(login_prompt_observer.handlers().empty());
  LoginHandler* handler = *login_prompt_observer.handlers().begin();
  // Note that the actual credentials don't matter because `fake_saml_idp()`
  // doesn't check those (only that something has been provided).
  handler->SetAuth(u"user", u"pwd");

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
}

// Tests the sign-in flow when the credentials passing API is used.
IN_PROC_BROWSER_TEST_F(SamlTest, CredentialPassingAPI) {
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

  EXPECT_TRUE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 1, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Provider", 1, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);
}

// Tests the sign-in flow when the credentials passing API is used w/o 'confirm'
// call. The password from the last 'add' should be used.
IN_PROC_BROWSER_TEST_F(SamlTest, CredentialPassingAPIWithoutConfirm) {
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

  EXPECT_TRUE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 1, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Provider", 1, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);
}

// Tests the single password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedSingle) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  content::DOMMessageQueue message_queue;
  // Make sure that the password is scraped correctly.
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "$('gaia-signin').authenticator_.addEventListener('authCompleted',"
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

  EXPECT_FALSE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      1, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);
}

// Tests password scraping from a dynamically created password field.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedDynamic) {
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

  EXPECT_FALSE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));
}

// Tests the multiple password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedMultiple) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TypeIntoPath("password1", {"Password1"});
  SigninFrameJS().TapOn("Submit");
  // Lands on confirm password screen.
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  // Entering an unknown password should go back to the confirm password screen.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  // Either scraped password should be able to sign-in.
  SendConfirmPassword("password1");
  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      2, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);
}

// Tests the no password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedNone) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_no_passwords.html");

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TapOn("Submit");

  // Lands on confirm password screen with manual input state.
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectTrue("$('saml-confirm-password').isManualInput");
  // Entering passwords that don't match will make us land again in the same
  // page.
  SetManualPasswords("Test1", "Test2");
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectTrue("$('saml-confirm-password').isManualInput");

  // Two matching passwords should let the user to sign in.
  SetManualPasswords("Test1", "Test1");
  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      0, 1);
  histogram_tester.ExpectTotalCount("OOBE.GaiaLoginTime", 0);
}

// Types the second user e-mail into the GAIA login form but then authenticates
// as the first user via SAML. Verifies that the logged-in user is correctly
// identified as the first user.
IN_PROC_BROWSER_TEST_F(SamlTest, UseAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  // Type the second user e-mail into the GAIA login form.
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSecondUserCorpExampleComEmail);

  // Authenticate as the first user via SAML (the `Email` provided here is
  // irrelevant - the authenticated user's e-mail address that FakeGAIA reports
  // was set via `SetFakeMergeSessionParams`).
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
IN_PROC_BROWSER_TEST_F(SamlTest, FailToRetrieveAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams("", kTestAuthSIDCookie1,
                                                    kTestAuthLSIDCookie1);
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  ExpectFatalErrorMessage(
      l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS));
}

// Tests the password confirm flow when more than one password is scraped: show
// error on the first failure and fatal error on the second failure.
IN_PROC_BROWSER_TEST_F(SamlTest, PasswordConfirmFlow) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TypeIntoPath("password1", {"Password1"});
  SigninFrameJS().TapOn("Submit");

  // Lands on confirm password screen with no error message.
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  test::OobeJS().ExpectTrue(
      "!$('saml-confirm-password').$.passwordInput.invalid");

  // Enter an unknown password for the first time should go back to confirm
  // password screen with error message.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  test::OobeJS().ExpectTrue(
      "$('saml-confirm-password').$.passwordInput.invalid");

  // Enter an unknown password 2nd time should go back fatal error message.
  SendConfirmPassword("wrong_password");
  ExpectFatalErrorMessage(
      l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_PASSWORD_VERIFICATION));
}

// Verifies that when the login flow redirects from one host to another, the
// notice shown to the user is updated. This guards against regressions of
// http://crbug.com/447818.
IN_PROC_BROWSER_TEST_F(SamlTest, NoticeUpdatedOnRedirect) {
  // Start another https server at `kAdditionalIdPHost`.
  HTTPSForwarder saml_https_forwarder;
  ASSERT_TRUE(saml_https_forwarder.Initialize(
      kAdditionalIdPHost, embedded_test_server()->base_url()));

  // Make the login flow redirect to `kAdditionalIdPHost`.
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_instant_meta_refresh.html");
  fake_saml_idp()->SetRefreshURL(
      saml_https_forwarder.GetURLForSSLHost("simple.html"));
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Wait until the notice shown to the user is updated to contain
  // `kAdditionalIdPHost`.
  std::string js =
      "var sendIfHostFound = function() {"
      "  var found = $SamlNoticeMessagePath.textContent.indexOf('$Host') > -1;"
      "  if (found)"
      "    window.domAutomationController.send(true);"
      "  return found;"
      "};"
      "var processEventsAndSendIfHostFound = function() {"
      "  window.setTimeout(function() {"
      "    if (sendIfHostFound()) {"
      "      $('gaia-signin').authenticator_.removeEventListener("
      "          'authDomainChange',"
      "          processEventsAndSendIfHostFound);"
      "    }"
      "  }, 0);"
      "};"
      "if (!sendIfHostFound()) {"
      "  $('gaia-signin').authenticator_.addEventListener("
      "      'authDomainChange',"
      "      processEventsAndSendIfHostFound);"
      "}";
  base::ReplaceSubstringsAfterOffset(
      &js, 0, "$SamlNoticeMessagePath",
      test::GetOobeElementPath(kSamlNoticeMessage));
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Host", kAdditionalIdPHost);
  bool dummy;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetLoginUI()->GetWebContents(), js, &dummy));

  // Verify that the notice is visible.
  test::OobeJS().ExpectVisiblePath(kSamlNoticeContainer);
}

// Verifies that when GAIA attempts to redirect to a SAML IdP served over http,
// not https, the redirect is blocked and an error message is shown.
IN_PROC_BROWSER_TEST_F(SamlTest, HTTPRedirectDisallowed) {
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
}

// Verifies that when GAIA attempts to redirect to a page served over http, not
// https, via an HTML meta refresh, the redirect is blocked and an error message
// is shown. This guards against regressions of http://crbug.com/359515.
IN_PROC_BROWSER_TEST_F(SamlTest, MetaRefreshToHTTPDisallowed) {
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
}

class SAMLEnrollmentTest : public SamlTest {
 public:
  SAMLEnrollmentTest();
  ~SAMLEnrollmentTest() override;

  // SamlTest:
  void SetUpOnMainThread() override;
  void StartSamlAndWaitForIdpPageLoad(const std::string& gaia_email) override;

 protected:
  LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(SAMLEnrollmentTest);
};

SAMLEnrollmentTest::SAMLEnrollmentTest() {
  gaia_frame_parent_ = "authView";
  authenticator_id_ = "$('enterprise-enrollment').authenticator_";
}

SAMLEnrollmentTest::~SAMLEnrollmentTest() {}

void SAMLEnrollmentTest::SetUpOnMainThread() {
  FakeGaia::AccessTokenInfo token_info;
  token_info.token = kTestUserinfoToken;
  token_info.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  token_info.scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
  token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  token_info.email = saml_test_users::kFirstUserCorpExampleComEmail;
  fake_gaia_.fake_gaia()->IssueOAuthToken(kTestRefreshToken, token_info);

  SamlTest::SetUpOnMainThread();
}

void SAMLEnrollmentTest::StartSamlAndWaitForIdpPageLoad(
    const std::string& gaia_email) {
  LoginDisplayHost::default_host()->StartWizard(
      EnrollmentScreenView::kScreenId);
  WaitForGaiaPageBackButtonUpdate();
  auto flow_change_waiter =
      OobeBaseTest::CreateGaiaPageEventWaiter("authFlowChange");
  SigninFrameJS().TypeIntoPath(gaia_email, {"identifier"});
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

class SAMLPolicyTest : public SamlTest {
 public:
  SAMLPolicyTest();
  ~SAMLPolicyTest() override;

  // SamlTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  void SetSAMLOfflineSigninTimeLimitPolicy(int limit);
  void EnableTransferSAMLCookiesPolicy();
  void SetLoginBehaviorPolicyToSAMLInterstitial();
  void SetLoginVideoCaptureAllowedUrls(const std::vector<GURL>& allowed);

  void ShowGAIALoginForm();
  void ShowSAMLInterstitial();
  void ClickBackOnSAMLInterstitialPage();
  void ClickNextOnSAMLInterstitialPage();
  void LogInWithSAML(const std::string& user_id,
                     const std::string& auth_sid_cookie,
                     const std::string& auth_lsid_cookie);

  std::string GetCookieValue(const std::string& name);

  void GetCookies();

 protected:
  policy::DevicePolicyCrosTestHelper test_helper_;
  policy::DevicePolicyBuilder* device_policy_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  net::CookieList cookie_list_;

  // Add a fake user so the login screen does not show GAIA auth by default.
  // This enables tests to control when the GAIA is shown (and ensure it's
  // loaded after SAML config has been set up).
  chromeos::LoginManagerMixin login_manager_{
      &mixin_host_,
      {chromeos::LoginManagerMixin::TestUserInfo(
          AccountId::FromUserEmailGaiaId("user@gmail.com", "1111"))}};

 private:
  DISALLOW_COPY_AND_ASSIGN(SAMLPolicyTest);
};

SAMLPolicyTest::SAMLPolicyTest()
    : device_policy_(test_helper_.device_policy()) {
  device_state_.SetState(
      DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  device_state_.set_skip_initial_policy_setup(true);
}

SAMLPolicyTest::~SAMLPolicyTest() {}

void SAMLPolicyTest::SetUpInProcessBrowserTestFixture() {
  SessionManagerClient::InitializeFakeInMemory();

  SamlTest::SetUpInProcessBrowserTestFixture();

  // Initialize device policy.
  std::set<std::string> device_affiliation_ids;
  device_affiliation_ids.insert(kAffiliationID);
  auto affiliation_helper = policy::AffiliationTestHelper::CreateForCloud(
      FakeSessionManagerClient::Get());
  ASSERT_NO_FATAL_FAILURE((affiliation_helper.SetDeviceAffiliationIDs(
      &test_helper_, device_affiliation_ids)));

  // Initialize user policy.
  ON_CALL(provider_, IsInitializationComplete(testing::_))
      .WillByDefault(testing::Return(true));
  ON_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
      .WillByDefault(testing::Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void SAMLPolicyTest::SetUpOnMainThread() {
  SamlTest::SetUpOnMainThread();

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
  std::set<std::string> user_affiliation_ids;
  user_affiliation_ids.insert(kAffiliationID);
  ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail, kFirstSAMLUserGaiaId),
      user_affiliation_ids);
  ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kSecondUserCorpExampleComEmail,
          kSecondSAMLUserGaiaId),
      user_affiliation_ids);
  ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kThirdUserCorpExampleComEmail, kThirdSAMLUserGaiaId),
      user_affiliation_ids);
  ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(kNonSAMLUserEmail, kNonSAMLUserGaiaId),
      user_affiliation_ids);

  // Set up fake networks.
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->SetupDefaultEnvironment();
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
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_saml_settings()->set_transfer_saml_cookies(true);

  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      CrosSettings::Get()->AddSettingsObserver(kAccountsPrefTransferSAMLCookies,
                                               run_loop.QuitClosure());
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  FakeSessionManagerClient::Get()->set_device_policy(device_policy_->GetBlob());
  FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  run_loop.Run();
}

void SAMLPolicyTest::SetLoginBehaviorPolicyToSAMLInterstitial() {
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_login_authentication_behavior()
      ->set_login_authentication_behavior(
          em::LoginAuthenticationBehaviorProto_LoginBehavior_SAML_INTERSTITIAL);

  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      CrosSettings::Get()->AddSettingsObserver(kLoginAuthenticationBehavior,
                                               run_loop.QuitClosure());
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  FakeSessionManagerClient::Get()->set_device_policy(device_policy_->GetBlob());
  FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  run_loop.Run();
}

void SAMLPolicyTest::SetLoginVideoCaptureAllowedUrls(
    const std::vector<GURL>& allowed) {
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  for (const GURL& url : allowed)
    proto.mutable_login_video_capture_allowed_urls()->add_urls(url.spec());

  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      CrosSettings::Get()->AddSettingsObserver(kLoginVideoCaptureAllowedUrls,
                                               run_loop.QuitClosure());
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  FakeSessionManagerClient::Get()->set_device_policy(device_policy_->GetBlob());
  FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  run_loop.Run();
}

void SAMLPolicyTest::ShowGAIALoginForm() {
  content::DOMMessageQueue message_queue;
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "$('gaia-signin').authenticator_.addEventListener('ready', function() {"
      "  window.domAutomationController.send('ready');"
      "});"));
  ASSERT_TRUE(ash::LoginScreenTestApi::ClickAddUserButton());
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"ready\"");
}

void SAMLPolicyTest::ShowSAMLInterstitial() {
  WaitForOobeUI();
  ASSERT_TRUE(ash::LoginScreenTestApi::ClickAddUserButton());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"gaia-signin", "saml-interstitial"})
      ->Wait();
}

void SAMLPolicyTest::ClickBackOnSAMLInterstitialPage() {
  test::OobeJS().TapOnPath({"gaia-signin", "interstitial-back"});
}

void SAMLPolicyTest::ClickNextOnSAMLInterstitialPage() {
  content::DOMMessageQueue message_queue;
  SetupAuthFlowChangeListener();

  test::OobeJS().TapOnPath({"gaia-signin", "interstitial-next"});

  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"SamlLoaded\"");
}

void SAMLPolicyTest::LogInWithSAML(const std::string& user_id,
                                   const std::string& auth_sid_cookie,
                                   const std::string& auth_lsid_cookie) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(user_id);

  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(user_id, auth_sid_cookie,
                                                    auth_lsid_cookie);
  fake_gaia_.SetupFakeGaiaForLogin(user_id, "", kTestRefreshToken);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  // Scraping a single password should finish the login right away.
  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();
}

std::string SAMLPolicyTest::GetCookieValue(const std::string& name) {
  for (net::CookieList::const_iterator it = cookie_list_.begin();
       it != cookie_list_.end(); ++it) {
    if (it->Name() == name)
      return it->Value();
  }
  return std::string();
}

void SAMLPolicyTest::GetCookies() {
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(
      user_manager::UserManager::Get()->GetActiveUser());
  ASSERT_TRUE(profile);
  base::RunLoop run_loop;
  content::BrowserContext::GetDefaultStoragePartition(profile)
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

  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
      kNonSAMLUserEmail, FakeGaiaMixin::kFakeSIDCookie,
      FakeGaiaMixin::kFakeLSIDCookie);
  fake_gaia_.SetupFakeGaiaForLogin(kNonSAMLUserEmail, "", kTestRefreshToken);

  // Log in without SAML.
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(kNonSAMLUserEmail, "password", "[]");

  test::WaitForPrimaryUserSessionStart();
}

// Verifies that the offline login time limit does not affect a user who
// authenticated without SAML.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, NoSAML) {
  // Verify that offline login is allowed.
  ash::LoginScreenTestApi::SubmitPassword(
      AccountId::FromUserEmail(kNonSAMLUserEmail), "password",
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
  // Verify that offline login is allowed.
  ash::LoginScreenTestApi::SubmitPassword(
      AccountId::FromUserEmail(saml_test_users::kFirstUserCorpExampleComEmail),
      "password", true /* check_if_submittable */);
  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SAMLZeroLimit) {
  // Set the offline login time limit for SAML users to zero.
  SetSAMLOfflineSigninTimeLimitPolicy(0);

  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
}

// Verifies that when the offline login time limit is exceeded for a user who
// authenticated via SAML, that user is forced to log in online the next time.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLZeroLimit) {
  // Verify that offline login is not allowed.
  ASSERT_TRUE(
      ash::LoginScreenTestApi::IsForcedOnlineSignin(AccountId::FromUserEmail(
          saml_test_users::kFirstUserCorpExampleComEmail)));
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

// Tests that the SAML interstitial page is loaded when the authentication
// behavior device policy is set to SAML_INTERSTITIAL, and when the user clicks
// the "change account" link, we go back to the default GAIA signin screen.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLInterstitialChangeAccount) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  SetLoginBehaviorPolicyToSAMLInterstitial();
  WaitForSigninScreen();

  ShowSAMLInterstitial();
  test::OobeJS().ExpectHiddenPath({"gaia-signin", "signin-frame-dialog"});
  test::OobeJS().ExpectVisiblePath({"gaia-signin", "saml-interstitial"});

  // Click the "change account" link on the SAML interstitial page.
  test::OobeJS().TapLinkOnPath({"gaia-signin", "interstitial-change-account"});

  // Expects that only the gaia signin frame is visible and shown.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"gaia-signin", "signin-frame-dialog"})
      ->Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(false, {"gaia-signin", "gaia-loading"})
      ->Wait();
  test::OobeJS().ExpectHasNoAttribute("transparent",
                                      {"gaia-signin", "signin-frame-dialog"});
  test::OobeJS().ExpectHiddenPath({"gaia-signin", "saml-interstitial"});
}

// Tests that clicking "Next" in the SAML interstitial page successfully
// triggers a SAML redirect request, and the SAML IdP authentication page is
// loaded and authenticaing there is successful.
// TODO(https://crbug.com/1102738) flaky test
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, DISABLED_SAMLInterstitialNext) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
      saml_test_users::kFirstUserCorpExampleComEmail, kTestAuthSIDCookie1,
      kTestAuthLSIDCookie1);
  SetLoginBehaviorPolicyToSAMLInterstitial();
  WaitForSigninScreen();

  ShowSAMLInterstitial();
  ClickBackOnSAMLInterstitialPage();
  // Back button should hide OOBE dialog.
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  ShowSAMLInterstitial();
  ClickNextOnSAMLInterstitialPage();

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  // Scraping one password should finish login.
  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();
}

// Ensure that the permission status of getUserMedia requests from SAML login
// pages is controlled by the kLoginVideoCaptureAllowedUrls pref rather than the
// underlying user content setting.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, TestLoginMediaPermission) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  const GURL url1("https://google.com");
  const GURL url2("https://corp.example.com");
  const GURL url3("https://not-allowed.com");
  SetLoginVideoCaptureAllowedUrls({url1, url2});
  WaitForSigninScreen();

  content::WebContents* web_contents = GetLoginUI()->GetWebContents();
  content::WebContentsDelegate* web_contents_delegate =
      web_contents->GetDelegate();

  // Mic should always be blocked.
  EXPECT_FALSE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetMainFrame(), url1,
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));

  // Camera should be allowed if allowed by the allowlist, otherwise blocked.
  EXPECT_TRUE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetMainFrame(), url1,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  EXPECT_TRUE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetMainFrame(), url2,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  EXPECT_FALSE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetMainFrame(), url3,
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
      web_contents->GetMainFrame(), url3,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}

class SAMLPasswordAttributesTest : public SAMLPolicyTest,
                                   public testing::WithParamInterface<bool> {
 public:
  SAMLPasswordAttributesTest() = default;
  void SetUpOnMainThread() override;

 protected:
  bool in_session_pw_change_policy_enabled() { return GetParam(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(SAMLPasswordAttributesTest);
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
    EXPECT_EQ(base::Time::FromJsTime(1550836258421L), attrs.modified_time());
    EXPECT_EQ(base::Time::FromJsTime(1551873058421L), attrs.expiration_time());
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
IN_PROC_BROWSER_TEST_P(SAMLPasswordAttributesTest, LoginFailed) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  fake_saml_idp()->SetSamlResponseFile("saml_with_password_attributes.xml");
  ShowGAIALoginForm();
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Give fake gaia an empty email address, so login will fail:
  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
      /*email=*/"", kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  // SAML login fails:
  ExpectFatalErrorMessage(
      l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS));

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

constexpr base::TimeDelta kTimeoutTaskDelay =
    base::TimeDelta::FromMilliseconds(500);
constexpr base::TimeDelta kBuildResponseTaskDelay =
    base::TimeDelta::FromSeconds(3);
static_assert(
    kTimeoutTaskDelay < kBuildResponseTaskDelay,
    "kTimeoutTaskDelay should be less than kBuildResponseTaskDelay to trigger "
    "timeout error in SAMLDeviceAttestationTest.TimeoutError test.");

class SAMLDeviceAttestationTest : public SamlTest {
 public:
  SAMLDeviceAttestationTest() = default;

  SAMLDeviceAttestationTest(const SAMLDeviceAttestationTest&) = delete;
  SAMLDeviceAttestationTest& operator=(const SAMLDeviceAttestationTest&) =
      delete;

  void SetUpInProcessBrowserTestFixture() override;

 protected:
  void SetAllowedUrlsPolicy(const std::vector<std::string>& allowed_urls);

  chromeos::ScopedTestingCrosSettings settings_helper_;
  StubCrosSettingsProvider* settings_provider_ = nullptr;

  attestation::MockMachineCertificateUploader mock_cert_uploader_;
  NiceMock<chromeos::attestation::MockAttestationFlow> mock_attestation_flow_;
  chromeos::ScopedStubInstallAttributes stub_install_attributes_;
};

void SAMLDeviceAttestationTest::SetUpInProcessBrowserTestFixture() {
  SamlTest::SetUpInProcessBrowserTestFixture();

  settings_provider_ = settings_helper_.device_settings();

  ON_CALL(mock_attestation_flow_, GetCertificate)
      .WillByDefault(WithArgs<5>(Invoke(FakeGetCertificateCallbackTrue)));

  // By default make it reply that the certificate is already uploaded.
  ON_CALL(mock_cert_uploader_, WaitForUploadComplete)
      .WillByDefault(RunOnceCallback<0>(/*certificate_uploaded=*/true));

  attestation::TpmChallengeKeyFactory::SetForTesting(
      std::make_unique<attestation::TpmChallengeKeyImpl>(
          &mock_attestation_flow_, &mock_cert_uploader_));

  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
}

void SAMLDeviceAttestationTest::SetAllowedUrlsPolicy(
    const std::vector<std::string>& allowed_urls) {
  std::vector<base::Value> allowed_urls_values;
  for (const auto& url : allowed_urls) {
    allowed_urls_values.push_back(base::Value(url));
  }
  settings_provider_->Set(chromeos::kDeviceWebBasedAttestationAllowedUrls,
                          base::Value(std::move(allowed_urls_values)));
}

// Verify that device attestation is not available when
// DeviceWebBasedAttestationAllowedUrls policy is not set.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, DefaultPolicy) {
  base::HistogramTester histogram_tester;

  // Leave policy unset.

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(kSamlChallengeKeyHandlerResultMetric,
                                      attestation::TpmChallengeKeyResultCode::
                                          kDeviceWebBasedAttestationUrlError,
                                      1);
}

// Verify that device attestation is not available when
// DeviceWebBasedAttestationAllowedUrls policy is set to empty list of allowed
// URLs.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, EmptyPolicy) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({/* empty list */});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(kSamlChallengeKeyHandlerResultMetric,
                                      attestation::TpmChallengeKeyResultCode::
                                          kDeviceWebBasedAttestationUrlError,
                                      1);
}

// Verify that device attestation is not available when device is not enterprise
// enrolled.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, NotEnterpriseEnrolledError) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({fake_saml_idp()->GetIdpHost()});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kNonEnterpriseDeviceError, 1);
}

// Verify that device attestation is not available when device attestation is
// not enabled.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest,
                       DeviceAttestationNotEnabledError) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({fake_saml_idp()->GetIdpHost()});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kDevicePolicyDisabledError, 1);
}

// Verify that device attestation works when all policies configured correctly.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, Success) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({fake_saml_idp()->GetIdpHost()});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  settings_provider_->SetBoolean(chromeos::kDeviceAttestationEnabled, true);

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_TRUE(fake_saml_idp()->IsLastChallengeResponseExists());
  ASSERT_NO_FATAL_FAILURE(
      fake_saml_idp()->AssertChallengeResponseMatchesTpmResponse());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kSuccess, 1);
}

// Verify that device attestation is not available for URLs that are not in the
// allowed URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, PolicyNoMatchError) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({fake_saml_idp()->GetIdpDomain()});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  settings_provider_->SetBoolean(chromeos::kDeviceAttestationEnabled, true);

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(kSamlChallengeKeyHandlerResultMetric,
                                      attestation::TpmChallengeKeyResultCode::
                                          kDeviceWebBasedAttestationUrlError,
                                      1);
}

// Verify that device attestation is available for URLs that match a pattern
// from allowed URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, PolicyRegexSuccess) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({"[*.]" + fake_saml_idp()->GetIdpDomain()});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  settings_provider_->SetBoolean(chromeos::kDeviceAttestationEnabled, true);

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_TRUE(fake_saml_idp()->IsLastChallengeResponseExists());
  ASSERT_NO_FATAL_FAILURE(
      fake_saml_idp()->AssertChallengeResponseMatchesTpmResponse());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kSuccess, 1);
}

// Verify that device attestation works in case of multiple items in allowed
// URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, PolicyTwoEntriesSuccess) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({"example2.com", fake_saml_idp()->GetIdpHost()});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  settings_provider_->SetBoolean(chromeos::kDeviceAttestationEnabled, true);

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_TRUE(fake_saml_idp()->IsLastChallengeResponseExists());
  ASSERT_NO_FATAL_FAILURE(
      fake_saml_idp()->AssertChallengeResponseMatchesTpmResponse());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, TimeoutError) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({"example2.com", fake_saml_idp()->GetIdpHost()});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  settings_provider_->SetBoolean(chromeos::kDeviceAttestationEnabled, true);

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

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kTimeoutError, 1);
}

}  // namespace chromeos
