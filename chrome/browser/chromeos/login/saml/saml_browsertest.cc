// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/https_forwarder.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/features/feature_channel.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace em = enterprise_management;

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using testing::_;
using testing::Return;

namespace chromeos {

namespace {

constexpr char kGAIASIDCookieName[] = "SID";
constexpr char kGAIALSIDCookieName[] = "LSID";

constexpr char kTestAuthSIDCookie1[] = "fake-auth-SID-cookie-1";
constexpr char kTestAuthSIDCookie2[] = "fake-auth-SID-cookie-2";
constexpr char kTestAuthLSIDCookie1[] = "fake-auth-LSID-cookie-1";
constexpr char kTestAuthLSIDCookie2[] = "fake-auth-LSID-cookie-2";

// Note: SAML account cannot be @gmail or @example.com account.  The former by
// design, the latter because @example.com is used in another tests as regular
// user. So we use @corp.example.com and @example.test, so that we can handle
// it specially in embedded_setup_chromeos.html .
constexpr char kFirstSAMLUserEmail[] = "bob@corp.example.com";
constexpr char kSecondSAMLUserEmail[] = "alice@corp.example.com";
constexpr char kHTTPSAMLUserEmail[] = "carol@corp.example.com";
constexpr char kNonSAMLUserEmail[] = "dan@corp.example.com";
constexpr char kDifferentDomainSAMLUserEmail[] = "eve@example.test";

constexpr char kFirstSAMLUserGaiaId[] = "bob-gaia";
constexpr char kSecondSAMLUserGaiaId[] = "alice-gaia";
constexpr char kHTTPSAMLUserGaiaId[] = "carol-gaia";
constexpr char kNonSAMLUserGaiaId[] = "dan-gaia";
constexpr char kDifferentDomainSAMLUserGaiaId[] = "eve-gaia";

constexpr char kIdPHost[] = "login.corp.example.com";
constexpr char kAdditionalIdPHost[] = "login2.corp.example.com";

constexpr char kSAMLIdPCookieName[] = "saml";
constexpr char kSAMLIdPCookieValue1[] = "value-1";
constexpr char kSAMLIdPCookieValue2[] = "value-2";

constexpr char kRelayState[] = "RelayState";

constexpr char kTestUserinfoToken[] = "fake-userinfo-token";
constexpr char kTestRefreshToken[] = "fake-refresh-token";
constexpr char kPolicy[] = "{\"managed_users\": [\"*\"]}";

constexpr char kAffiliationID[] = "some-affiliation-id";

// FakeSamlIdp serves IdP auth form and the form submission. The form is
// served with the template's RelayState placeholder expanded to the real
// RelayState parameter from request. The form submission redirects back to
// FakeGaia with the same RelayState.
class FakeSamlIdp {
 public:
  FakeSamlIdp();
  ~FakeSamlIdp();

  void SetUp(const std::string& base_path, const GURL& gaia_url);

  void SetLoginHTMLTemplate(const std::string& template_file);
  void SetLoginAuthHTMLTemplate(const std::string& template_file);
  void SetRefreshURL(const GURL& refresh_url);
  void SetCookieValue(const std::string& cookie_value);

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

 private:
  std::unique_ptr<HttpResponse> BuildHTMLResponse(
      const std::string& html_template,
      const std::string& relay_state,
      const std::string& next_path);

  base::FilePath html_template_dir_;

  std::string login_path_;
  std::string login_auth_path_;

  std::string login_html_template_;
  std::string login_auth_html_template_;
  GURL gaia_assertion_url_;
  GURL refresh_url_;
  std::string cookie_value_;

  DISALLOW_COPY_AND_ASSIGN(FakeSamlIdp);
};

FakeSamlIdp::FakeSamlIdp() {}

FakeSamlIdp::~FakeSamlIdp() {}

void FakeSamlIdp::SetUp(const std::string& base_path, const GURL& gaia_url) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  html_template_dir_ = test_data_dir.Append("login");

  login_path_ = base_path;
  login_auth_path_ = base_path + "Auth";
  gaia_assertion_url_ = gaia_url.Resolve("/SSO");
}

void FakeSamlIdp::SetLoginHTMLTemplate(const std::string& template_file) {
  EXPECT_TRUE(base::ReadFileToString(html_template_dir_.Append(template_file),
                                     &login_html_template_));
}

void FakeSamlIdp::SetLoginAuthHTMLTemplate(const std::string& template_file) {
  EXPECT_TRUE(base::ReadFileToString(html_template_dir_.Append(template_file),
                                     &login_auth_html_template_));
}

void FakeSamlIdp::SetRefreshURL(const GURL& refresh_url) {
  refresh_url_ = refresh_url;
}

void FakeSamlIdp::SetCookieValue(const std::string& cookie_value) {
  cookie_value_ = cookie_value;
}

std::unique_ptr<HttpResponse> FakeSamlIdp::HandleRequest(
    const HttpRequest& request) {
  // The scheme and host of the URL is actually not important but required to
  // get a valid GURL in order to parse |request.relative_url|.
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_path = request_url.path();

  if (request_path == login_path_) {
    std::string relay_state;
    net::GetValueForKeyInQuery(request_url, kRelayState, &relay_state);
    return BuildHTMLResponse(login_html_template_, relay_state,
                             login_auth_path_);
  }

  if (request_path != login_auth_path_) {
    // Request not understood.
    return std::unique_ptr<HttpResponse>();
  }

  std::string relay_state;
  FakeGaia::GetQueryParameter(request.content, kRelayState, &relay_state);
  GURL redirect_url = gaia_assertion_url_;

  if (!login_auth_html_template_.empty()) {
    return BuildHTMLResponse(login_auth_html_template_, relay_state,
                             redirect_url.spec());
  }

  redirect_url =
      net::AppendQueryParameter(redirect_url, "SAMLResponse", "fake_response");
  redirect_url =
      net::AppendQueryParameter(redirect_url, kRelayState, relay_state);

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  http_response->AddCustomHeader(
      "Set-cookie", base::StringPrintf("saml=%s", cookie_value_.c_str()));
  return std::move(http_response);
}

std::unique_ptr<HttpResponse> FakeSamlIdp::BuildHTMLResponse(
    const std::string& html_template,
    const std::string& relay_state,
    const std::string& next_path) {
  std::string response_html = html_template;
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$RelayState",
                                     relay_state);
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$Post", next_path);
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$Refresh",
                                     refresh_url_.spec());

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response_html);
  http_response->set_content_type("text/html");

  return std::move(http_response);
}

// A FakeCryptohomeClient that stores the salted and hashed secret passed to
// MountEx().
class SecretInterceptingFakeCryptohomeClient : public FakeCryptohomeClient {
 public:
  SecretInterceptingFakeCryptohomeClient();

  void MountEx(const cryptohome::AccountIdentifier& id,
               const cryptohome::AuthorizationRequest& auth,
               const cryptohome::MountRequest& request,
               DBusMethodCallback<cryptohome::BaseReply> callback) override;

  const std::string& salted_hashed_secret() { return salted_hashed_secret_; }

 private:
  std::string salted_hashed_secret_;

  DISALLOW_COPY_AND_ASSIGN(SecretInterceptingFakeCryptohomeClient);
};

SecretInterceptingFakeCryptohomeClient::
    SecretInterceptingFakeCryptohomeClient() {}

void SecretInterceptingFakeCryptohomeClient::MountEx(
    const cryptohome::AccountIdentifier& id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::MountRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  salted_hashed_secret_ = auth.key().secret();
  FakeCryptohomeClient::MountEx(id, auth, request, std::move(callback));
}

}  // namespace

class SamlTest : public OobeBaseTest {
 public:
  SamlTest() : cryptohome_client_(new SecretInterceptingFakeCryptohomeClient) {
    set_initialize_fake_merge_session(false);
  }
  ~SamlTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    command_line->AppendSwitch(
        chromeos::switches::kAllowFailedPolicyFetchForTest);

    const GURL gaia_url = gaia_https_forwarder_.GetURLForSSLHost("");
    const GURL saml_idp_url = saml_https_forwarder_.GetURLForSSLHost("SAML");
    fake_saml_idp_.SetUp(saml_idp_url.path(), gaia_url);
    fake_gaia_->RegisterSamlUser(kFirstSAMLUserEmail, saml_idp_url);
    fake_gaia_->RegisterSamlUser(kSecondSAMLUserEmail, saml_idp_url);
    fake_gaia_->RegisterSamlUser(
        kHTTPSAMLUserEmail,
        embedded_test_server()->base_url().Resolve("/SAML"));
    fake_gaia_->RegisterSamlUser(kDifferentDomainSAMLUserEmail, saml_idp_url);
    fake_gaia_->RegisterSamlDomainRedirectUrl("corp.example.com", saml_idp_url);

    OobeBaseTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
        std::unique_ptr<CryptohomeClient>(cryptohome_client_));

    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    fake_gaia_->SetFakeMergeSessionParams(
        kFirstSAMLUserEmail, kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

    embedded_test_server()->RegisterRequestHandler(base::Bind(
        &FakeSamlIdp::HandleRequest, base::Unretained(&fake_saml_idp_)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void SetupAuthFlowChangeListener() {
    content::ExecuteScriptAsync(
        GetLoginUI()->GetWebContents(),
        "$('gaia-signin').gaiaAuthHost_.addEventListener('authFlowChange',"
        "    function f() {"
        "      $('gaia-signin').gaiaAuthHost_.removeEventListener("
        "          'authFlowChange', f);"
        "      window.domAutomationController.send("
        "          $('gaia-signin').isSAML() ? 'SamlLoaded' : 'GaiaLoaded');"
        "    });");
  }

  virtual void StartSamlAndWaitForIdpPageLoad(const std::string& gaia_email) {
    WaitForSigninScreen();

    SetupAuthFlowChangeListener();

    content::DOMMessageQueue message_queue;  // Start observe before SAML.
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetGaiaScreenView()
        ->ShowSigninScreenForTest(gaia_email, "", "[]");

    std::string message;
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"SamlLoaded\"", message);
  }

  void SendConfirmPassword(const std::string& password_to_confirm) {
    std::string js =
        "$('saml-confirm-password').$.passwordInput.value='$Password';"
        "$('saml-confirm-password').$.inputForm.submit();";
    base::ReplaceSubstringsAfterOffset(&js, 0, "$Password",
                                       password_to_confirm);
    ASSERT_TRUE(content::ExecuteScript(GetLoginUI()->GetWebContents(), js));
  }

  void SetManualPasswords(const std::string& password,
                          const std::string& confirm_password) {
    std::string js =
        "$('saml-confirm-password').$.passwordInput.value='$Password';"
        "$('saml-confirm-password').$$('#confirmPasswordInput').value="
        "    '$ConfirmPassword';"
        "$('saml-confirm-password').$.inputForm.submit();";
    base::ReplaceSubstringsAfterOffset(&js, 0, "$Password", password);
    base::ReplaceSubstringsAfterOffset(&js, 0, "$ConfirmPassword",
                                       confirm_password);
    ASSERT_TRUE(content::ExecuteScript(GetLoginUI()->GetWebContents(), js));
  }

  std::string WaitForAndGetFatalErrorMessage() {
    OobeScreenWaiter(OobeScreen::SCREEN_FATAL_ERROR).Wait();
    std::string message_element = "$('fatal-error-card')";
    std::string error_message;
    if (!content::ExecuteScriptAndExtractString(
            GetLoginUI()->GetWebContents(),
            "window.domAutomationController.send(" + message_element +
                ".textContent);",
            &error_message)) {
      ADD_FAILURE();
    }
    return error_message;
  }

  FakeSamlIdp* fake_saml_idp() { return &fake_saml_idp_; }

 protected:
  void InitHttpsForwarders() override {
    ASSERT_TRUE(saml_https_forwarder_.Initialize(
        kIdPHost, embedded_test_server()->base_url()));
    OobeBaseTest::InitHttpsForwarders();
  }

  HTTPSForwarder saml_https_forwarder_;

  SecretInterceptingFakeCryptohomeClient* cryptohome_client_;

 private:
  FakeSamlIdp fake_saml_idp_;

  DISALLOW_COPY_AND_ASSIGN(SamlTest);
};

// Tests that signin frame should have 'saml' class and 'cancel' button is
// visible when SAML IdP page is loaded. And 'cancel' button goes back to
// gaia on clicking.
//
// Times out on CrOS MSAN. https://crbug.com/504141
// Times out on CrOS ASAN/LSAN. https://crbug.com/830322
#if defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(ADDRESS_SANITIZER)
#define MAYBE_SamlUI DISABLED_SamlUI
#else
#define MAYBE_SamlUI SamlUI
#endif
IN_PROC_BROWSER_TEST_F(SamlTest, MAYBE_SamlUI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  // Saml flow UI expectations.
  JsExpect("$('gaia-signin').classList.contains('full-width')");
  JsExpect("!$('saml-notice-container').hidden");
  std::string js = "$('saml-notice-message').textContent.indexOf('$Host') > -1";
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Host", kIdPHost);
  JsExpect(js);

  SetupAuthFlowChangeListener();

  // Click on 'cancel'.
  content::DOMMessageQueue message_queue;  // Observe before 'cancel'.
  content::ExecuteScriptAsync(GetLoginUI()->GetWebContents(),
                              "$('gaia-navigation').$.closeButton.click();");

  // Auth flow should change back to Gaia.
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"GaiaLoaded\"");

  // Saml flow is gone.
  JsExpect("!$('gaia-signin').classList.contains('full-width')");
}

// Tests the sign-in flow when the credentials passing API is used.
//
// Flaky. See http://crbug.com/659992
#if defined(OS_LINUX)
#define MAYBE_CredentialPassingAPI DISABLED_CredentialPassingAPI
#else
#define MAYBE_CredentialPassingAPI CredentialPassingAPI
#endif
IN_PROC_BROWSER_TEST_F(SamlTest, MAYBE_CredentialPassingAPI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Dummy", "not_the_password");
  SetSignFormField("Password", "actual_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  // Login should finish login and a session should start.
  session_start_waiter.Wait();

  // Regression test for http://crbug.com/490737: Verify that the user's actual
  // password was used, not the contents of the first type=password input field
  // found on the page.
  Key key("actual_password");
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                SystemSaltGetter::ConvertRawSaltToHexString(
                    FakeCryptohomeClient::GetStubSystemSalt()));
  EXPECT_EQ(key.GetSecret(), cryptohome_client_->salted_hashed_secret());
}

// Tests the single password scraped flow.
//
// Disabled since it's occasionally timed out: https://crbug.com/830322.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_ScrapedSingle DISABLED_ScrapedSingle
#else
#define MAYBE_ScrapedSingle ScrapedSingle
#endif
IN_PROC_BROWSER_TEST_F(SamlTest, MAYBE_ScrapedSingle) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  // Make sure that the password is scraped correctly.
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "$('gaia-signin').gaiaAuthHost_.addEventListener('authCompleted',"
      "    function(e) {"
      "      var password = e.detail.password;"
      "      window.domAutomationController.send(password);"
      "    });"));

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");

  // Scraping a single password should finish the login and start the session.
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  content::DOMMessageQueue message_queue;
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"fake_password\"");

  session_start_waiter.Wait();
}

// Tests password scraping from a dynamically created password field.
//
// Disabled since it's occasionally timed out: https://crbug.com/830322.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_ScrapedDynamic DISABLED_ScrapedDynamic
#else
#define MAYBE_ScrapedDynamic ScrapedDynamic
#endif
IN_PROC_BROWSER_TEST_F(SamlTest, MAYBE_ScrapedDynamic) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  ExecuteJsInSigninFrame(
      "(function() {"
      "  var newPassInput = document.createElement('input');"
      "  newPassInput.id = 'DynamicallyCreatedPassword';"
      "  newPassInput.type = 'password';"
      "  newPassInput.name = 'Password';"
      "  document.forms[0].appendChild(newPassInput);"
      "})();");

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("DynamicallyCreatedPassword", "fake_password");

  // Scraping a single password should finish the login and start the session.
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");
  session_start_waiter.Wait();
}

// Tests the multiple password scraped flow.
//
// Disabled due to flakiness: crbug.com/834703
IN_PROC_BROWSER_TEST_F(SamlTest, DISABLED_ScrapedMultiple) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");

  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  SetSignFormField("Password1", "password1");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  // Lands on confirm password screen.
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  JsExpect("!$('saml-confirm-password').manualInput");

  // Entering an unknown password should go back to the confirm password screen.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  JsExpect("!$('saml-confirm-password').manualInput");

  // Either scraped password should be able to sign-in.
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  SendConfirmPassword("password1");
  session_start_waiter.Wait();
}

// Tests the no password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedNone) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_no_passwords.html");

  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  SetSignFormField("Email", "fake_user");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  // Lands on confirm password screen with manual input state.
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  JsExpect("$('saml-confirm-password').manualInput");
  // Entering passwords that don't match will make us land again in the same
  // page.
  SetManualPasswords("Test1", "Test2");
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  JsExpect("$('saml-confirm-password').manualInput");

  // Two matching passwords should let the user to sign in.
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  SetManualPasswords("Test1", "Test1");
  session_start_waiter.Wait();
}

// Types |bob@corp.example.com| into the GAIA login form but then authenticates
// as |alice@corp.example.com| via SAML. Verifies that the logged-in user is
// correctly identified as Alice.
//
// Disabled since it's occasionally timed out: https://crbug.com/830322.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_UseAutenticatedUserEmailAddress \
  DISABLED_UseAutenticatedUserEmailAddress
#else
#define MAYBE_UseAutenticatedUserEmailAddress UseAutenticatedUserEmailAddress
#endif
IN_PROC_BROWSER_TEST_F(SamlTest, MAYBE_UseAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  // Type |bob@corp.example.com| into the GAIA login form.
  StartSamlAndWaitForIdpPageLoad(kSecondSAMLUserEmail);

  // Authenticate as alice@corp.example.com via SAML (the |Email| provided here
  // is irrelevant - the authenticated user's e-mail address that FakeGAIA
  // reports was set via |SetFakeMergeSessionParams|).
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");
  session_start_waiter.Wait();

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  ASSERT_TRUE(user);
  EXPECT_EQ(kFirstSAMLUserEmail, user->GetAccountId().GetUserEmail());
}

// Verifies that if the authenticated user's e-mail address cannot be retrieved,
// an error message is shown.
IN_PROC_BROWSER_TEST_F(SamlTest, FailToRetrieveAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  fake_gaia_->SetFakeMergeSessionParams("", kTestAuthSIDCookie1,
                                        kTestAuthLSIDCookie1);
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS),
            WaitForAndGetFatalErrorMessage());
}

// Tests the password confirm flow when more than one password is scraped: show
// error on the first failure and fatal error on the second failure.
//
// Disabled since it's occasionally timed out: https://crbug.com/830322.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_PasswordConfirmFlow DISABLED_PasswordConfirmFlow
#else
#define MAYBE_PasswordConfirmFlow PasswordConfirmFlow
#endif
IN_PROC_BROWSER_TEST_F(SamlTest, MAYBE_PasswordConfirmFlow) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  SetSignFormField("Password1", "password1");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  // Lands on confirm password screen with no error message.
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  JsExpect("!$('saml-confirm-password').manualInput");
  JsExpect("!$('saml-confirm-password').$.passwordInput.isInvalid");

  // Enter an unknown password for the first time should go back to confirm
  // password screen with error message.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  JsExpect("!$('saml-confirm-password').manualInput");
  JsExpect("$('saml-confirm-password').$.passwordInput.isInvalid");

  // Enter an unknown password 2nd time should go back fatal error message.
  SendConfirmPassword("wrong_password");
  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_PASSWORD_VERIFICATION),
      WaitForAndGetFatalErrorMessage());
}

// Verifies that when the login flow redirects from one host to another, the
// notice shown to the user is updated. This guards against regressions of
// http://crbug.com/447818.
//
// Disabled since it's occasionally timed out: https://crbug.com/830322.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_NoticeUpdatedOnRedirect DISABLED_NoticeUpdatedOnRedirect
#else
#define MAYBE_NoticeUpdatedOnRedirect NoticeUpdatedOnRedirect
#endif
IN_PROC_BROWSER_TEST_F(SamlTest, MAYBE_NoticeUpdatedOnRedirect) {
  // Start another https server at |kAdditionalIdPHost|.
  HTTPSForwarder saml_https_forwarder_2;
  ASSERT_TRUE(saml_https_forwarder_2.Initialize(
      kAdditionalIdPHost, embedded_test_server()->base_url()));

  // Make the login flow redirect to |kAdditionalIdPHost|.
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_instant_meta_refresh.html");
  fake_saml_idp()->SetRefreshURL(
      saml_https_forwarder_2.GetURLForSSLHost("simple.html"));
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  // Wait until the notice shown to the user is updated to contain
  // |kAdditionalIdPHost|.
  std::string js =
      "var sendIfHostFound = function() {"
      "  var found ="
      "      $('saml-notice-message').textContent.indexOf('$Host') > -1;"
      "  if (found)"
      "    window.domAutomationController.send(true);"
      "  return found;"
      "};"
      "var processEventsAndSendIfHostFound = function() {"
      "  window.setTimeout(function() {"
      "    if (sendIfHostFound()) {"
      "      $('gaia-signin').gaiaAuthHost_.removeEventListener("
      "          'authDomainChange',"
      "          processEventsAndSendIfHostFound);"
      "    }"
      "  }, 0);"
      "};"
      "if (!sendIfHostFound()) {"
      "  $('gaia-signin').gaiaAuthHost_.addEventListener("
      "      'authDomainChange',"
      "      processEventsAndSendIfHostFound);"
      "}";
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Host", kAdditionalIdPHost);
  bool dummy;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetLoginUI()->GetWebContents(), js, &dummy));

  // Verify that the notice is visible.
  JsExpect("!$('saml-notice-container').hidden");
}

// Verifies that when GAIA attempts to redirect to a SAML IdP served over http,
// not https, the redirect is blocked and an error message is shown.
IN_PROC_BROWSER_TEST_F(SamlTest, HTTPRedirectDisallowed) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  WaitForSigninScreen();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetGaiaScreenView()
      ->ShowSigninScreenForTest(kHTTPSAMLUserEmail, "", "[]");

  const GURL url = embedded_test_server()->base_url().Resolve("/SAML");
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL,
                                      base::UTF8ToUTF16(url.spec())),
            WaitForAndGetFatalErrorMessage());
}

// Verifies that when GAIA attempts to redirect to a page served over http, not
// https, via an HTML meta refresh, the redirect is blocked and an error message
// is shown. This guards against regressions of http://crbug.com/359515.
//
// Disabled since it's occasionally timed out: https://crbug.com/830322.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_MetaRefreshToHTTPDisallowed DISABLED_MetaRefreshToHTTPDisallowed
#else
#define MAYBE_MetaRefreshToHTTPDisallowed MetaRefreshToHTTPDisallowed
#endif
IN_PROC_BROWSER_TEST_F(SamlTest, MAYBE_MetaRefreshToHTTPDisallowed) {
  const GURL url = embedded_test_server()->base_url().Resolve("/SSO");
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_instant_meta_refresh.html");
  fake_saml_idp()->SetRefreshURL(url);

  WaitForSigninScreen();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetGaiaScreenView()
      ->ShowSigninScreenForTest(kFirstSAMLUserEmail, "", "[]");

  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL,
                                      base::UTF8ToUTF16(url.spec())),
            WaitForAndGetFatalErrorMessage());
}

class SAMLEnrollmentTest : public SamlTest,
                           public content::WebContentsObserver {
 public:
  SAMLEnrollmentTest();
  ~SAMLEnrollmentTest() override;

  // SamlTest:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void StartSamlAndWaitForIdpPageLoad(const std::string& gaia_email) override;

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  void WaitForEnrollmentSuccess();
  guest_view::TestGuestViewManager* GetGuestViewManager();
  content::WebContents* GetEnrollmentContents();

 private:
  std::unique_ptr<policy::LocalPolicyTestServer> test_server_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<base::RunLoop> run_loop_;

  guest_view::TestGuestViewManagerFactory guest_view_manager_factory_;

  DISALLOW_COPY_AND_ASSIGN(SAMLEnrollmentTest);
};

SAMLEnrollmentTest::SAMLEnrollmentTest() {
  guest_view::GuestViewManager::set_factory_for_testing(
      &guest_view_manager_factory_);
  gaia_frame_parent_ = "oauth-enroll-auth-view";
}

SAMLEnrollmentTest::~SAMLEnrollmentTest() {}

void SAMLEnrollmentTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath policy_file =
      temp_dir_.GetPath().AppendASCII("policy.json");
  ASSERT_EQ(static_cast<int>(strlen(kPolicy)),
            base::WriteFile(policy_file, kPolicy, strlen(kPolicy)));

  test_server_.reset(new policy::LocalPolicyTestServer(policy_file));
  ASSERT_TRUE(test_server_->Start());

  SamlTest::SetUp();
}

void SAMLEnrollmentTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                  test_server_->GetServiceURL().spec());

  SamlTest::SetUpCommandLine(command_line);
}

void SAMLEnrollmentTest::SetUpOnMainThread() {
  FakeGaia::AccessTokenInfo token_info;
  token_info.token = kTestUserinfoToken;
  token_info.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  token_info.scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  token_info.email = kFirstSAMLUserEmail;
  fake_gaia_->IssueOAuthToken(kTestRefreshToken, token_info);

  SamlTest::SetUpOnMainThread();
}

void SAMLEnrollmentTest::StartSamlAndWaitForIdpPageLoad(
    const std::string& gaia_email) {
  WaitForSigninScreen();
  run_loop_.reset(new base::RunLoop);
  ExistingUserController::current_controller()->OnStartEnterpriseEnrollment();
  while (!GetEnrollmentContents()) {
    GetGuestViewManager()->WaitForNextGuestCreated();
  }
  Observe(GetEnrollmentContents());
  run_loop_->Run();

  SetSignFormField("identifier", gaia_email);

  run_loop_.reset(new base::RunLoop);
  ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");
  run_loop_->Run();
}

void SAMLEnrollmentTest::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  const GURL origin = validated_url.GetOrigin();
  if (origin != gaia_https_forwarder_.GetURLForSSLHost(std::string()) &&
      origin != saml_https_forwarder_.GetURLForSSLHost(std::string())) {
    return;
  }

  // The GAIA or SAML IdP login form finished loading.
  if (run_loop_)
    run_loop_->Quit();
}

// Waits until the class |oauth-enroll-state-success| becomes set for the
// enrollment screen, indicating enrollment success.
void SAMLEnrollmentTest::WaitForEnrollmentSuccess() {
  bool done = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetLoginUI()->GetWebContents(),
      "var enrollmentScreen = document.getElementById('oauth-enrollment');"
      "function SendReplyIfEnrollmentDone() {"
      "  if (enrollmentScreen.classList.contains("
      "          'oauth-enroll-state-attribute-prompt')) {"
      "    $('oauth-enroll-attribute-prompt-card').fire('submit');"
      "    return false;"
      "  }"
      "  if (!enrollmentScreen.classList.contains("
      "           'oauth-enroll-state-success')) {"
      "    return false;"
      "  }"
      "  domAutomationController.send(true);"
      "  observer.disconnect();"
      "  return true;"
      "}"
      "var observer = new MutationObserver(SendReplyIfEnrollmentDone);"
      "if (!SendReplyIfEnrollmentDone()) {"
      "  var options = { attributes: true, attributeFilter: [ 'class' ] };"
      "  observer.observe(enrollmentScreen, options);"
      "}",
      &done));
}

guest_view::TestGuestViewManager* SAMLEnrollmentTest::GetGuestViewManager() {
  using namespace guest_view;
  return static_cast<TestGuestViewManager*>(
      TestGuestViewManager::FromBrowserContext(
          ProfileHelper::GetSigninProfile()));
}

content::WebContents* SAMLEnrollmentTest::GetEnrollmentContents() {
  content::RenderFrameHost* frame_host =
      signin::GetAuthFrame(GetLoginUI()->GetWebContents(), gaia_frame_parent_);
  if (!frame_host)
    return nullptr;
  return content::WebContents::FromRenderFrameHost(frame_host);
}

// Flaky. See crbug.com/766953.
#if defined(ADDRESS_SANITIZER) || defined(OS_CHROMEOS)
#define MAYBE_WithoutCredentialsPassingAPI DISABLED_WithoutCredentialsPassingAPI
#else
#define MAYBE_WithoutCredentialsPassingAPI WithoutCredentialsPassingAPI
#endif
IN_PROC_BROWSER_TEST_F(SAMLEnrollmentTest, MAYBE_WithoutCredentialsPassingAPI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  WaitForEnrollmentSuccess();
}

// Flaky. See crbug.com/766953.
#if defined(ADDRESS_SANITIZER) || defined(OS_CHROMEOS)
#define MAYBE_WithCredentialsPassingAPI DISABLED_WithCredentialsPassingAPI
#else
#define MAYBE_WithCredentialsPassingAPI WithCredentialsPassingAPI
#endif
IN_PROC_BROWSER_TEST_F(SAMLEnrollmentTest, MAYBE_WithCredentialsPassingAPI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  WaitForEnrollmentSuccess();
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
  void ClickNextOnSAMLInterstitialPage();
  void ClickChangeAccountOnSAMLInterstitialPage();
  void LogInWithSAML(const std::string& user_id,
                     const std::string& auth_sid_cookie,
                     const std::string& auth_lsid_cookie);

  std::string GetCookieValue(const std::string& name);

  void GetCookies();

 protected:
  policy::DevicePolicyCrosTestHelper test_helper_;

  // FakeDBusThreadManager uses FakeSessionManagerClient.
  FakeSessionManagerClient* fake_session_manager_client_;
  policy::DevicePolicyBuilder* device_policy_;

  policy::MockConfigurationPolicyProvider provider_;

  net::CookieList cookie_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SAMLPolicyTest);
};

SAMLPolicyTest::SAMLPolicyTest()
    : fake_session_manager_client_(new FakeSessionManagerClient),
      device_policy_(test_helper_.device_policy()) {}

SAMLPolicyTest::~SAMLPolicyTest() {}

void SAMLPolicyTest::SetUpInProcessBrowserTestFixture() {
  DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
      std::unique_ptr<SessionManagerClient>(fake_session_manager_client_));

  SamlTest::SetUpInProcessBrowserTestFixture();

  // Initialize device policy.
  std::set<std::string> device_affiliation_ids;
  device_affiliation_ids.insert(kAffiliationID);
  auto affiliation_helper = policy::AffiliationTestHelper::CreateForCloud(
      fake_session_manager_client_);
  ASSERT_NO_FATAL_FAILURE((affiliation_helper.SetDeviceAffiliationIDs(
      &test_helper_, device_affiliation_ids)));

  // Initialize user policy.
  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void SAMLPolicyTest::SetUpOnMainThread() {
  SamlTest::SetUpOnMainThread();

  // Pretend that the test users' OAuth tokens are valid.
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmailGaiaId(kFirstSAMLUserEmail, kFirstSAMLUserGaiaId),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmailGaiaId(kNonSAMLUserEmail, kNonSAMLUserGaiaId),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmailGaiaId(kDifferentDomainSAMLUserEmail,
                                     kDifferentDomainSAMLUserGaiaId),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

  // Give affiliated users appropriate affiliation IDs.
  std::set<std::string> user_affiliation_ids;
  user_affiliation_ids.insert(kAffiliationID);
  chromeos::ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(kFirstSAMLUserEmail, kFirstSAMLUserGaiaId),
      user_affiliation_ids);
  chromeos::ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(kSecondSAMLUserEmail,
                                     kSecondSAMLUserGaiaId),
      user_affiliation_ids);
  chromeos::ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(kHTTPSAMLUserEmail, kHTTPSAMLUserGaiaId),
      user_affiliation_ids);
  chromeos::ChromeUserManager::Get()->SetUserAffiliation(
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
                  policy::POLICY_SOURCE_CLOUD,
                  std::make_unique<base::Value>(limit), nullptr);
  provider_.UpdateChromePolicy(user_policy);
  base::RunLoop().RunUntilIdle();
}

void SAMLPolicyTest::EnableTransferSAMLCookiesPolicy() {
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_saml_settings()->set_transfer_saml_cookies(true);

  base::RunLoop run_loop;
  std::unique_ptr<CrosSettings::ObserverSubscription> observer =
      CrosSettings::Get()->AddSettingsObserver(kAccountsPrefTransferSAMLCookies,
                                               run_loop.QuitClosure());
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  fake_session_manager_client_->set_device_policy(device_policy_->GetBlob());
  fake_session_manager_client_->OnPropertyChangeComplete(true);
  run_loop.Run();
}

void SAMLPolicyTest::SetLoginBehaviorPolicyToSAMLInterstitial() {
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_login_authentication_behavior()
      ->set_login_authentication_behavior(
          em::LoginAuthenticationBehaviorProto_LoginBehavior_SAML_INTERSTITIAL);

  base::RunLoop run_loop;
  std::unique_ptr<CrosSettings::ObserverSubscription> observer =
      CrosSettings::Get()->AddSettingsObserver(kLoginAuthenticationBehavior,
                                               run_loop.QuitClosure());
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  fake_session_manager_client_->set_device_policy(device_policy_->GetBlob());
  fake_session_manager_client_->OnPropertyChangeComplete(true);
  run_loop.Run();
}

void SAMLPolicyTest::SetLoginVideoCaptureAllowedUrls(
    const std::vector<GURL>& allowed) {
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  for (const GURL& url : allowed)
    proto.mutable_login_video_capture_allowed_urls()->add_urls(url.spec());

  base::RunLoop run_loop;
  std::unique_ptr<CrosSettings::ObserverSubscription> observer =
      CrosSettings::Get()->AddSettingsObserver(kLoginVideoCaptureAllowedUrls,
                                               run_loop.QuitClosure());
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  fake_session_manager_client_->set_device_policy(device_policy_->GetBlob());
  fake_session_manager_client_->OnPropertyChangeComplete(true);
  run_loop.Run();
}

void SAMLPolicyTest::ShowGAIALoginForm() {
  login_screen_load_observer_->Wait();
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "$('gaia-signin').gaiaAuthHost_.addEventListener('ready', function() {"
      "  window.domAutomationController.send('ready');"
      "});"
      "$('add-user-button').click();"));
  content::DOMMessageQueue message_queue;
  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"ready\"", message);
}

void SAMLPolicyTest::ShowSAMLInterstitial() {
  login_screen_load_observer_->Wait();
  ASSERT_TRUE(
      content::ExecuteScript(GetLoginUI()->GetWebContents(),
                             "$('saml-interstitial').addEventListener("
                             "    'samlInterstitialPageReady',"
                             "    function() {"
                             "        window.domAutomationController.send("
                             "            'samlInterstitialPageReady');"
                             "    });"
                             "$('add-user-button').click();"));

  content::DOMMessageQueue message_queue;
  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"samlInterstitialPageReady\"", message);
}

void SAMLPolicyTest::ClickNextOnSAMLInterstitialPage() {
  login_screen_load_observer_->Wait();

  content::DOMMessageQueue message_queue;
  SetupAuthFlowChangeListener();

  ASSERT_TRUE(content::ExecuteScript(GetLoginUI()->GetWebContents(),
                                     "$('saml-interstitial').submit();"));

  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"SamlLoaded\"");
  EXPECT_EQ("\"SamlLoaded\"", message);
}

void SAMLPolicyTest::ClickChangeAccountOnSAMLInterstitialPage() {
  login_screen_load_observer_->Wait();
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "$('gaia-signin').gaiaAuthHost_.addEventListener('ready', function() {"
      "  window.domAutomationController.send('ready');"
      "});"
      "$('saml-interstitial').changeAccountLink.click();"));

  content::DOMMessageQueue message_queue;
  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"ready\"", message);
}

void SAMLPolicyTest::LogInWithSAML(const std::string& user_id,
                                   const std::string& auth_sid_cookie,
                                   const std::string& auth_lsid_cookie) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(user_id);

  fake_gaia_->SetFakeMergeSessionParams(user_id, auth_sid_cookie,
                                        auth_lsid_cookie);
  SetupFakeGaiaForLogin(user_id, "", kTestRefreshToken);

  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");

  // Scraping a single password should finish the login right away.
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");
  session_start_waiter.Wait();
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
  network::mojom::CookieManagerPtr cookie_manager;
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

  fake_gaia_->SetFakeMergeSessionParams(kNonSAMLUserEmail, kFakeSIDCookie,
                                        kFakeLSIDCookie);
  SetupFakeGaiaForLogin(kNonSAMLUserEmail, "", kTestRefreshToken);

  // Log in without SAML.
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetGaiaScreenView()
      ->ShowSigninScreenForTest(kNonSAMLUserEmail, "password", "[]");

  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources())
      .Wait();
}

// Verifies that the offline login time limit does not affect a user who
// authenticated without SAML.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, NoSAML) {
  login_screen_load_observer_->Wait();
  // Verify that offline login is allowed.
  JsExpect(
      "window.getComputedStyle(document.querySelector("
      "    '#pod-row .reauth-hint-container')).display == 'none'");
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SAMLNoLimit) {
  // Remove the offline login time limit for SAML users.
  SetSAMLOfflineSigninTimeLimitPolicy(-1);

  LogInWithSAML(kFirstSAMLUserEmail, kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
}

// Verifies that when no offline login time limit is set, a user who
// authenticated with SAML is allowed to log in offline.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLNoLimit) {
  login_screen_load_observer_->Wait();
  // Verify that offline login is allowed.
  JsExpect(
      "window.getComputedStyle(document.querySelector("
      "    '#pod-row .reauth-hint-container')).display == 'none'");
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SAMLZeroLimit) {
  // Set the offline login time limit for SAML users to zero.
  SetSAMLOfflineSigninTimeLimitPolicy(0);

  LogInWithSAML(kFirstSAMLUserEmail, kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
}

// Verifies that when the offline login time limit is exceeded for a user who
// authenticated via SAML, that user is forced to log in online the next time.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLZeroLimit) {
  login_screen_load_observer_->Wait();
  // Verify that offline login is not allowed.
  JsExpect(
      "window.getComputedStyle(document.querySelector("
      "    '#pod-row .reauth-hint-container')).display != 'none'");
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_PRE_TransferCookiesAffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue1);
  LogInWithSAML(kFirstSAMLUserEmail, kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

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
  LogInWithSAML(kFirstSAMLUserEmail, kTestAuthSIDCookie2, kTestAuthLSIDCookie2);

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
  LogInWithSAML(kFirstSAMLUserEmail, kTestAuthSIDCookie2, kTestAuthLSIDCookie2);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue2, GetCookieValue(kSAMLIdPCookieName));
}

// PRE_TransferCookiesUnaffiliated and TransferCookiesUnaffiliated are flaky on
// MSAN, ASAN, Debug due to time out - most likely, because of the general
// slowness of the test. See crbug.com/683161, crbug.com/714167.
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
#define MAYBE_PRE_TransferCookiesUnaffiliated \
  DISABLED_PRE_TransferCookiesUnaffiliated
#define MAYBE_TransferCookiesUnaffiliated DISABLED_TransferCookiesUnaffiliated
#else
#define MAYBE_PRE_TransferCookiesUnaffiliated PRE_TransferCookiesUnaffiliated
#define MAYBE_TransferCookiesUnaffiliated TransferCookiesUnaffiliated
#endif

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, MAYBE_PRE_TransferCookiesUnaffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue1);
  LogInWithSAML(kDifferentDomainSAMLUserEmail, kTestAuthSIDCookie1,
                kTestAuthLSIDCookie1);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Verifies that even if the DeviceTransferSAMLCookies policy is enabled, SAML
// IdP are not transferred to a user's profile on subsequent login if the user
// does not belong to the domain that the device is enrolled into. Also verifies
// that GAIA cookies are not transferred.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, MAYBE_TransferCookiesUnaffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue2);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  ShowGAIALoginForm();

  EnableTransferSAMLCookiesPolicy();
  LogInWithSAML(kDifferentDomainSAMLUserEmail, kTestAuthSIDCookie1,
                kTestAuthLSIDCookie1);

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
  JsExpect("$('signin-frame').hidden == true");
  JsExpect("$('offline-gaia').hidden == true");
  JsExpect("$('saml-interstitial').hidden == false");

  // Click the "change account" link on the SAML interstitial page.
  ClickChangeAccountOnSAMLInterstitialPage();

  // Expects that only the gaia signin frame is visible and shown.
  JsExpect("$('signin-frame').classList.contains('show')");
  JsExpect("$('signin-frame').hidden == false");
  JsExpect("$('offline-gaia').hidden == true");
  JsExpect("$('saml-interstitial').hidden == true");
}

// Tests that clicking "Next" in the SAML interstitial page successfully
// triggers a SAML redirect request, and the SAML IdP authentication page is
// loaded and authenticaing there is successful.
// Disabled due to flakiness, see crbug.com/699228
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, DISABLED_SAMLInterstitialNext) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  fake_gaia_->SetFakeMergeSessionParams(
      kFirstSAMLUserEmail, kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
  SetLoginBehaviorPolicyToSAMLInterstitial();
  WaitForSigninScreen();

  ShowSAMLInterstitial();
  ClickNextOnSAMLInterstitialPage();

  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");

  // Scraping one password should finish login.
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");
  session_start_waiter.Wait();
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

  // Mic should always be blocked.
  EXPECT_FALSE(
      MediaCaptureDevicesDispatcher::GetInstance()->CheckMediaAccessPermission(
          web_contents->GetMainFrame(), url1,
          content::MEDIA_DEVICE_AUDIO_CAPTURE));

  // Camera should be allowed if allowed by the whitelist, otherwise blocked.
  EXPECT_TRUE(
      MediaCaptureDevicesDispatcher::GetInstance()->CheckMediaAccessPermission(
          web_contents->GetMainFrame(), url1,
          content::MEDIA_DEVICE_VIDEO_CAPTURE));

  EXPECT_TRUE(
      MediaCaptureDevicesDispatcher::GetInstance()->CheckMediaAccessPermission(
          web_contents->GetMainFrame(), url2,
          content::MEDIA_DEVICE_VIDEO_CAPTURE));

  EXPECT_FALSE(
      MediaCaptureDevicesDispatcher::GetInstance()->CheckMediaAccessPermission(
          web_contents->GetMainFrame(), url3,
          content::MEDIA_DEVICE_VIDEO_CAPTURE));

  // Camera should be blocked in the login screen, even if it's allowed via
  // content setting.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(url3, url3,
                                      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                                      std::string(), CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(
      MediaCaptureDevicesDispatcher::GetInstance()->CheckMediaAccessPermission(
          web_contents->GetMainFrame(), url3,
          content::MEDIA_DEVICE_VIDEO_CAPTURE));
}

}  // namespace chromeos
